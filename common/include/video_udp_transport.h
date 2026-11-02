#ifndef INCLUDE_VIDEO_UDP_TRANSPORT_H
#define INCLUDE_VIDEO_UDP_TRANSPORT_H

#include "video_frame.h"

#include <cstdint>
#include <netinet/in.h>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace media {
    class VideoUdpTransport {
    public:
        explicit VideoUdpTransport(std::uint16_t port = 0);
        ~VideoUdpTransport();
        VideoUdpTransport(const VideoUdpTransport&) = delete;
        VideoUdpTransport& operator=(const VideoUdpTransport&) = delete;

        bool start();
        void close();
        std::uint16_t port() const;
        void registerParticipant(std::uint32_t sessionId,
                                 std::uint32_t senderId,
                                 const sockaddr_in& endpoint);
        void unregisterParticipant(std::uint32_t sessionId,
                                   std::uint32_t senderId);
        std::optional<VideoFrame> receive(int timeoutMilliseconds);
        std::size_t forward(const VideoFrame& frame);

    private:
        std::uint16_t requestedPort;
        int socketFd = -1;
        std::uint16_t boundPort = 0;
        std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> senders;
        std::unordered_map<std::uint32_t,
                           std::unordered_map<std::uint32_t, sockaddr_in>> endpoints;
    };
}

#endif // INCLUDE_VIDEO_UDP_TRANSPORT_H
