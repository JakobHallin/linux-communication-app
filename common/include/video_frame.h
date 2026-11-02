#ifndef INCLUDE_VIDEO_FRAME_H
#define INCLUDE_VIDEO_FRAME_H

#include <cstdint>
#include <optional>
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
}

#endif // INCLUDE_VIDEO_FRAME_H
