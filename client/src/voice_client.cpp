#include "voice_client.h"

#include "audio_client_session.h"
#include "audio_codec.h"
#include "audio_jitter_buffer.h"
#include "audio_packet.h"
#include "audio_stream_mixer.h"

#include <alsa/asoundlib.h>

#include <arpa/inet.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <vector>

namespace client {
    struct VoiceClient::Impl {
        std::atomic_bool running{false};
        std::atomic_bool muted{false};
        std::thread captureThread;
        std::thread playbackThread;
    };

    namespace {
        void checkAlsa(int result, const char* operation) {
            if (result < 0) {
                throw std::runtime_error(std::string(operation) + ": " + snd_strerror(result));
            }
        }

        void configurePcm(snd_pcm_t* pcm) {
            snd_pcm_hw_params_t* params = nullptr;
            checkAlsa(snd_pcm_hw_params_malloc(&params), "allocate ALSA parameters");
            try {
                checkAlsa(snd_pcm_hw_params_any(pcm, params), "initialize ALSA parameters");
                checkAlsa(snd_pcm_hw_params_set_access(pcm, params,
                    SND_PCM_ACCESS_RW_INTERLEAVED), "set ALSA access mode");
                checkAlsa(snd_pcm_hw_params_set_format(pcm, params,
                    SND_PCM_FORMAT_S16_LE), "set ALSA sample format");
                unsigned int rate = media::OpusCodec::sampleRate;
                checkAlsa(snd_pcm_hw_params_set_rate_near(pcm, params, &rate, nullptr),
                          "set ALSA sample rate");
                if (rate != media::OpusCodec::sampleRate) {
                    throw std::runtime_error("ALSA device does not support 48000 Hz");
                }
                unsigned int channels = 1;
                checkAlsa(snd_pcm_hw_params_set_channels_near(pcm, params, &channels),
                          "set ALSA channel count");
                if (channels != 1) throw std::runtime_error("ALSA device did not accept mono audio");
                snd_pcm_uframes_t period = media::OpusCodec::frameSamples;
                checkAlsa(snd_pcm_hw_params_set_period_size_near(pcm, params, &period, nullptr),
                          "set ALSA period size");
                checkAlsa(snd_pcm_hw_params(pcm, params), "apply ALSA parameters");
            } catch (...) {
                snd_pcm_hw_params_free(params);
                throw;
            }
            snd_pcm_hw_params_free(params);
        }

        void recoverPcm(snd_pcm_t* pcm, snd_pcm_sframes_t result) {
            if (result == -EPIPE || result == -ESTRPIPE) {
                checkAlsa(snd_pcm_prepare(pcm), "recover ALSA stream");
                return;
            }
            checkAlsa(static_cast<int>(result), "ALSA I/O");
        }

        void writeFrame(snd_pcm_t* playback, const std::vector<std::int16_t>& frame,
                        const std::atomic_bool& running) {
            snd_pcm_sframes_t written = 0;
            while (running && written < static_cast<snd_pcm_sframes_t>(frame.size())) {
                const auto result = snd_pcm_writei(playback, frame.data() + written,
                    frame.size() - static_cast<std::size_t>(written));
                if (result == -EAGAIN) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                } else if (result < 0) {
                    recoverPcm(playback, result);
                }
                else written += result;
            }
        }
    }

    VoiceClient::VoiceClient(std::string host, std::uint16_t port,
                             std::uint32_t session, std::uint32_t sender,
                             std::string capture, std::string playback)
        : serverHost(std::move(host)), mediaPort(port), sessionId(session), senderId(sender),
          captureDevice(std::move(capture)), playbackDevice(std::move(playback)), impl(new Impl) {}

    VoiceClient::~VoiceClient() {
        stop();
        delete impl;
    }

    bool VoiceClient::start() {
        if (running) return true;
        socketFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socketFd < 0) return false;
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = 0;
        if (bind(socketFd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
            stop();
            return false;
        }
        socklen_t length = sizeof(local);
        if (getsockname(socketFd, reinterpret_cast<sockaddr*>(&local), &length) < 0) {
            stop();
            return false;
        }
        boundPort = ntohs(local.sin_port);
        running = true;
        return true;
    }

    void VoiceClient::activate() {
        if (!running || impl->running) return;
        impl->running = true;
        impl->captureThread = std::thread(&VoiceClient::captureLoop, this);
        impl->playbackThread = std::thread(&VoiceClient::playbackLoop, this);
    }

    void VoiceClient::stop() {
        if (!impl) return;
        impl->running = false;
        running = false;
        if (socketFd >= 0) {
            ::close(socketFd);
            socketFd = -1;
        }
        if (impl->captureThread.joinable()) impl->captureThread.join();
        if (impl->playbackThread.joinable()) impl->playbackThread.join();
    }

    void VoiceClient::setMuted(bool muted) { impl->muted = muted; }

    std::uint16_t VoiceClient::localPort() const { return boundPort; }

    void VoiceClient::setMediaPort(std::uint16_t port) { mediaPort = port; }

    void VoiceClient::setIdentity(std::uint32_t session, std::uint32_t sender) {
        sessionId = session;
        senderId = sender;
    }

    void VoiceClient::captureLoop() {
        snd_pcm_t* capture = nullptr;
        try {
            checkAlsa(snd_pcm_open(&capture, captureDevice.c_str(), SND_PCM_STREAM_CAPTURE,
                                   SND_PCM_NONBLOCK),
                      "open capture device");
            configurePcm(capture);
            std::cerr << "Voice capture ready: " << captureDevice << '\n';
            media::OpusCodec codec;
            AudioClientSession session(sessionId, senderId);
            sockaddr_in server{};
            server.sin_family = AF_INET;
            server.sin_port = htons(mediaPort);
            if (inet_pton(AF_INET, serverHost.c_str(), &server.sin_addr) != 1) {
                throw std::runtime_error("invalid voice server address");
            }
            std::vector<std::int16_t> samples(media::OpusCodec::frameSamples);
            std::size_t collected = 0;
            std::size_t sentPackets = 0;
            while (impl->running) {
                const auto result = snd_pcm_readi(capture, samples.data() + collected,
                                                  samples.size() - collected);
                if (result == -EAGAIN) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                if (result < 0) { recoverPcm(capture, result); continue; }
                collected += static_cast<std::size_t>(result);
                if (collected < samples.size()) continue;
                collected = 0;
                const auto encoded = codec.encode(samples);
                session.setMuted(impl->muted);
                const auto packet = session.makePacket(encoded);
                if (packet) {
                    const auto sent = sendto(socketFd, packet->data(), packet->size(), MSG_NOSIGNAL,
                                             reinterpret_cast<sockaddr*>(&server), sizeof(server));
                    if (sent == static_cast<ssize_t>(packet->size()) && ++sentPackets == 1) {
                        std::cerr << "Voice capture sent first packet\n";
                    }
                }
            }
        } catch (const std::exception& error) {
            std::cerr << "Voice capture stopped: " << error.what() << '\n';
        }
        if (capture) snd_pcm_close(capture);
    }

    void VoiceClient::playbackLoop() {
        snd_pcm_t* playback = nullptr;
        try {
            checkAlsa(snd_pcm_open(&playback, playbackDevice.c_str(), SND_PCM_STREAM_PLAYBACK,
                                   SND_PCM_NONBLOCK),
                      "open playback device");
            configurePcm(playback);
            std::cerr << "Voice playback ready: " << playbackDevice << '\n';
            media::OpusCodec codec;
            std::unordered_map<std::uint32_t, media::AudioJitterBuffer> jitterBuffers;
            media::AudioStreamMixer mixer;
            std::vector<std::uint8_t> bytes(media::maximumAudioPacketBytes);
            pollfd descriptor{socketFd, POLLIN, 0};
            auto nextPlayback = std::chrono::steady_clock::now();
            bool receivedPacket = false;
            while (impl->running) {
                const auto now = std::chrono::steady_clock::now();
                const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::max(std::chrono::milliseconds(0),
                             std::chrono::duration_cast<std::chrono::milliseconds>(
                                 nextPlayback - now))).count();
                if (poll(&descriptor, 1, static_cast<int>(std::min<std::int64_t>(wait, 10))) > 0) {
                    do {
                        const auto received = recvfrom(socketFd, bytes.data(), bytes.size(),
                                                       MSG_DONTWAIT, nullptr, nullptr);
                        if (received <= 0) break;
                        bytes.resize(static_cast<std::size_t>(received));
                        const auto packet = media::deserializeAudioPacket(bytes);
                        if (packet && packet->sessionId == sessionId && packet->senderId != senderId) {
                            if (!receivedPacket) {
                                std::cerr << "Voice playback received first packet\n";
                                receivedPacket = true;
                            }
                            const auto decoded = codec.decode(packet->payload);
                            if (decoded && decoded->size() == media::OpusCodec::frameSamples) {
                                jitterBuffers[packet->senderId].push(packet->sequence, *decoded);
                            }
                        }
                        bytes.resize(media::maximumAudioPacketBytes);
                    } while (impl->running);
                }

                if (std::chrono::steady_clock::now() >= nextPlayback) {
                    for (auto& [remoteSender, jitter] : jitterBuffers) {
                        const auto frame = jitter.popNext();
                        if (frame.has_value()) {
                            mixer.submit(remoteSender, *frame);
                        } else {
                            mixer.remove(remoteSender);
                        }
                    }
                    writeFrame(playback, mixer.mix(), impl->running);
                    nextPlayback += std::chrono::milliseconds(20);
                    if (nextPlayback < std::chrono::steady_clock::now()) {
                        nextPlayback = std::chrono::steady_clock::now();
                    }
                }
            }
        } catch (const std::exception& error) {
            std::cerr << "Voice playback stopped: " << error.what() << '\n';
        }
        if (playback) snd_pcm_close(playback);
    }
}
