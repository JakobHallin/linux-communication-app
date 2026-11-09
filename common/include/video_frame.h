#ifndef INCLUDE_VIDEO_FRAME_H
#define INCLUDE_VIDEO_FRAME_H

#include <cstdint>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace media {
    constexpr std::size_t maximumVideoFrameBytes = 1U << 20;
    constexpr std::size_t videoFrameHeaderBytes = 26;
    constexpr std::size_t maximumVideoDatagramBytes = 65'507;

    struct VideoFrame {
        std::uint32_t sessionId;
        std::uint32_t senderId;
        std::uint32_t sequence;
        std::uint16_t width;
        std::uint16_t height;
        std::vector<std::uint8_t> payload;
    };

    std::vector<std::uint8_t> serializeVideoFrame(const VideoFrame& frame);
    std::optional<VideoFrame> deserializeVideoFrame(
        const std::vector<std::uint8_t>& bytes);

    std::vector<std::vector<std::uint8_t>> serializeVideoFrameDatagrams(
        const VideoFrame& frame);

    class VideoFrameReassembler {
    public:
        std::optional<VideoFrame> accept(
            const std::vector<std::uint8_t>& datagram);

    private:
        struct Key {
            std::uint32_t sessionId;
            std::uint32_t senderId;
            std::uint32_t sequence;
            bool operator==(const Key&) const = default;
        };

        struct KeyHash {
            std::size_t operator()(const Key& key) const;
        };

        struct Pending {
            std::uint16_t width;
            std::uint16_t height;
            std::uint32_t totalBytes;
            std::vector<std::vector<std::uint8_t>> parts;
            std::size_t receivedParts = 0;
            std::chrono::steady_clock::time_point lastReceived;
        };

        std::unordered_map<Key, Pending, KeyHash> pending;
    };
}

#endif // INCLUDE_VIDEO_FRAME_H
