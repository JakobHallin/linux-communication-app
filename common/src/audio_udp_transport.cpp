#include "audio_udp_transport.h"

#include <arpa/inet.h>
#include <chrono>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>

namespace media {
    AudioUdpTransport::AudioUdpTransport(std::uint16_t port)
        : requestedPort(port) {}

    AudioUdpTransport::~AudioUdpTransport() { close(); }

    bool AudioUdpTransport::start() {
        if (socketFd >= 0) return true;
        socketFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socketFd < 0) return false;

        int reuse = 1;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            close();
            return false;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(requestedPort);
        if (bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            close();
            return false;
        }

        sockaddr_in boundAddress{};
        socklen_t addressLength = sizeof(boundAddress);
        if (getsockname(socketFd, reinterpret_cast<sockaddr*>(&boundAddress),
                        &addressLength) < 0) {
            close();
            return false;
        }
        boundPort = ntohs(boundAddress.sin_port);
        return true;
    }

    void AudioUdpTransport::close() {
        if (socketFd >= 0) {
            ::close(socketFd);
            socketFd = -1;
        }
        boundPort = 0;
    }

    std::uint16_t AudioUdpTransport::port() const { return boundPort; }

    void AudioUdpTransport::registerSender(std::uint32_t sessionId,
                                           std::uint32_t senderId) {
        sessions.registerSender(sessionId, senderId);
    }

    void AudioUdpTransport::registerParticipant(std::uint32_t sessionId,
                                                std::uint32_t senderId,
                                                const sockaddr_in& endpoint) {
        registerSender(sessionId, senderId);
        endpoints[sessionId][senderId] = endpoint;
    }

    void AudioUdpTransport::unregisterSender(std::uint32_t sessionId,
                                             std::uint32_t senderId) {
        sessions.unregisterSender(sessionId, senderId);
        const auto session = endpoints.find(sessionId);
        if (session == endpoints.end()) return;
        session->second.erase(senderId);
        if (session->second.empty()) endpoints.erase(session);
    }

    std::optional<AudioPacket> AudioUdpTransport::receive(int timeoutMilliseconds) {
        if (socketFd < 0 || timeoutMilliseconds < 0) return std::nullopt;

        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(timeoutMilliseconds);
        std::array<std::uint8_t, maximumAudioPacketBytes> buffer{};
        while (true) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining < 0) return std::nullopt;

            pollfd descriptor{socketFd, POLLIN, 0};
            const int ready = poll(&descriptor, 1, static_cast<int>(remaining));
            if (ready <= 0) return std::nullopt;

            const auto received = recvfrom(socketFd, buffer.data(), buffer.size(), 0,
                                           nullptr, nullptr);
            if (received <= 0) continue;
            const std::vector<std::uint8_t> bytes(buffer.begin(),
                                                  buffer.begin() + received);
            const auto packet = deserializeAudioPacket(bytes);
            if (packet && sessions.accepts(*packet)) return packet;
        }
    }

    std::size_t AudioUdpTransport::forward(const AudioPacket& packet) {
        if (socketFd < 0 || !sessions.accepts(packet)) return 0;
        const auto session = endpoints.find(packet.sessionId);
        if (session == endpoints.end()) return 0;

        const auto bytes = serializeAudioPacket(packet);
        if (bytes.empty()) return 0;

        std::size_t forwarded = 0;
        for (const auto& [participantId, endpoint] : session->second) {
            if (participantId == packet.senderId) continue;
            const auto sent = sendto(socketFd, bytes.data(), bytes.size(), 0,
                                     reinterpret_cast<const sockaddr*>(&endpoint),
                                     sizeof(endpoint));
            if (sent == static_cast<ssize_t>(bytes.size())) ++forwarded;
        }
        return forwarded;
    }
}
