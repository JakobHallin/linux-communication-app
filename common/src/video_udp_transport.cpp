#include "video_udp_transport.h"

#include <arpa/inet.h>
#include <chrono>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>

namespace media {
    namespace {
    }

    VideoUdpTransport::VideoUdpTransport(std::uint16_t port)
        : requestedPort(port) {}

    VideoUdpTransport::~VideoUdpTransport() { close(); }

    bool VideoUdpTransport::start() {
        if (socketFd >= 0) return true;
        socketFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socketFd < 0) return false;

        int reuse = 1;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR,
                       &reuse, sizeof(reuse)) < 0) {
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

    void VideoUdpTransport::close() {
        if (socketFd >= 0) {
            ::close(socketFd);
            socketFd = -1;
        }
        boundPort = 0;
    }

    std::uint16_t VideoUdpTransport::port() const { return boundPort; }

    void VideoUdpTransport::registerParticipant(
        std::uint32_t sessionId, std::uint32_t senderId,
        const sockaddr_in& endpoint) {
        senders[sessionId].insert(senderId);
        endpoints[sessionId][senderId] = endpoint;
    }

    void VideoUdpTransport::unregisterParticipant(
        std::uint32_t sessionId, std::uint32_t senderId) {
        const auto session = senders.find(sessionId);
        if (session != senders.end()) {
            session->second.erase(senderId);
            if (session->second.empty()) senders.erase(session);
        }

        const auto endpointSession = endpoints.find(sessionId);
        if (endpointSession == endpoints.end()) return;
        endpointSession->second.erase(senderId);
        if (endpointSession->second.empty()) endpoints.erase(endpointSession);
    }

    std::optional<VideoFrame> VideoUdpTransport::receive(int timeoutMilliseconds) {
        if (socketFd < 0 || timeoutMilliseconds < 0) return std::nullopt;

        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(timeoutMilliseconds);
        std::array<std::uint8_t, maximumVideoDatagramBytes> buffer{};
        while (true) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining < 0) return std::nullopt;

            pollfd descriptor{socketFd, POLLIN, 0};
            if (poll(&descriptor, 1, static_cast<int>(remaining)) <= 0) {
                return std::nullopt;
            }

            const auto received = recvfrom(socketFd, buffer.data(), buffer.size(),
                                           0, nullptr, nullptr);
            if (received <= 0) continue;
            const std::vector<std::uint8_t> bytes(buffer.begin(),
                                                  buffer.begin() + received);
            const auto frame = reassembler.accept(bytes);
            if (frame && senders.contains(frame->sessionId) &&
                senders.at(frame->sessionId).contains(frame->senderId)) {
                return frame;
            }
        }
    }

    std::size_t VideoUdpTransport::forward(const VideoFrame& frame) {
        if (socketFd < 0) return 0;
        const auto session = senders.find(frame.sessionId);
        if (session == senders.end() || !session->second.contains(frame.senderId)) {
            return 0;
        }

        const auto endpointSession = endpoints.find(frame.sessionId);
        if (endpointSession == endpoints.end()) return 0;
        const auto datagrams = serializeVideoFrameDatagrams(frame);
        if (datagrams.empty()) return 0;

        std::size_t forwarded = 0;
        for (const auto& [participantId, endpoint] : endpointSession->second) {
            if (participantId == frame.senderId) continue;
            bool delivered = true;
            for (const auto& datagram : datagrams) {
                const auto sent = sendto(socketFd, datagram.data(), datagram.size(), 0,
                                         reinterpret_cast<const sockaddr*>(&endpoint),
                                         sizeof(endpoint));
                if (sent != static_cast<ssize_t>(datagram.size())) {
                    delivered = false;
                    break;
                }
            }
            if (delivered) ++forwarded;
        }
        return forwarded;
    }
}
