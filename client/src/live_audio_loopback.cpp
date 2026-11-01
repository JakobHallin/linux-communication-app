#include "audio_codec.h"

#include <alsa/asoundlib.h>

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::atomic_bool running{true};

void stopLoopback(int) { running = false; }

void checkAlsa(int result, const char* operation) {
    if (result < 0) {
        throw std::runtime_error(std::string(operation) + ": " + snd_strerror(result));
    }
}

void configurePcm(snd_pcm_t* pcm, const char* operation) {
    snd_pcm_hw_params_t* params = nullptr;
    checkAlsa(snd_pcm_hw_params_malloc(&params), "allocate ALSA parameters");
    try {
        checkAlsa(snd_pcm_hw_params_any(pcm, params), "initialize ALSA parameters");
        checkAlsa(snd_pcm_hw_params_set_access(
                      pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED),
                  "set ALSA access mode");
        checkAlsa(snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE),
                  "set ALSA sample format");
        unsigned int sampleRate = media::OpusCodec::sampleRate;
        checkAlsa(snd_pcm_hw_params_set_rate_near(pcm, params, &sampleRate, nullptr),
                  "set ALSA sample rate");
        if (sampleRate != media::OpusCodec::sampleRate) {
            throw std::runtime_error(std::string(operation) + ": device does not support 48000 Hz");
        }
        unsigned int channels = 1;
        checkAlsa(snd_pcm_hw_params_set_channels_near(pcm, params, &channels),
                  "set ALSA channel count");
        if (channels != 1) {
            throw std::runtime_error(std::string(operation) + ": device did not accept mono audio");
        }
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

void recoverPcm(snd_pcm_t* pcm, int result, const char* operation) {
    if (result == -EPIPE || result == -ESTRPIPE) {
        checkAlsa(snd_pcm_prepare(pcm), operation);
        return;
    }
    checkAlsa(result, operation);
}

void writeAll(snd_pcm_t* playback, const std::vector<std::int16_t>& samples) {
    snd_pcm_sframes_t written = 0;
    while (written < static_cast<snd_pcm_sframes_t>(samples.size()) && running) {
        const auto* data = samples.data() + written;
        const auto remaining = samples.size() - static_cast<std::size_t>(written);
        const auto result = snd_pcm_writei(playback, data, remaining);
        if (result < 0) {
            recoverPcm(playback, static_cast<int>(result), "write audio");
        } else {
            written += result;
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    const char* captureDevice = argc > 1 ? argv[1] : "default";
    const char* playbackDevice = argc > 2 ? argv[2] : "default";
    snd_pcm_t* capture = nullptr;
    snd_pcm_t* playback = nullptr;

    try {
        checkAlsa(snd_pcm_open(&capture, captureDevice, SND_PCM_STREAM_CAPTURE, 0),
                  "open capture device");
        try {
            checkAlsa(snd_pcm_open(&playback, playbackDevice, SND_PCM_STREAM_PLAYBACK, 0),
                      "open playback device");
            configurePcm(capture, "configure capture device");
            configurePcm(playback, "configure playback device");
        } catch (...) {
            snd_pcm_close(capture);
            capture = nullptr;
            if (playback != nullptr) snd_pcm_close(playback);
            playback = nullptr;
            throw;
        }

        std::signal(SIGINT, stopLoopback);
        std::signal(SIGTERM, stopLoopback);
        std::cout << "Live audio loopback running.\n"
                  << "Capture: " << captureDevice << "\n"
                  << "Playback: " << playbackDevice << "\n"
                  << "Path: microphone -> Opus -> speaker\n"
                  << "Press Ctrl-C to stop.\n";

        media::OpusCodec codec;
        std::vector<std::int16_t> samples(media::OpusCodec::frameSamples);
        while (running) {
            const auto captured = snd_pcm_readi(capture, samples.data(), samples.size());
            if (captured < 0) {
                recoverPcm(capture, static_cast<int>(captured), "read audio");
                continue;
            }
            if (captured != static_cast<snd_pcm_sframes_t>(samples.size())) continue;

            const auto encoded = codec.encode(samples);
            if (encoded.empty()) throw std::runtime_error("Opus encoding failed");
            const auto decoded = codec.decode(encoded);
            if (!decoded.has_value() || decoded->size() != samples.size()) {
                throw std::runtime_error("Opus decoding failed");
            }
            writeAll(playback, *decoded);
        }

        snd_pcm_drop(capture);
        snd_pcm_drain(playback);
        snd_pcm_close(capture);
        snd_pcm_close(playback);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Live audio loopback failed: " << error.what() << '\n';
        if (capture != nullptr) snd_pcm_close(capture);
        if (playback != nullptr) snd_pcm_close(playback);
        return EXIT_FAILURE;
    }
}
