#ifndef INCLUDE_MJPEG_FRAME_H
#define INCLUDE_MJPEG_FRAME_H

#include "video_frame.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace media {
    bool isValidMjpeg(const std::vector<std::uint8_t>& payload);

    std::optional<VideoFrame> makeMjpegVideoFrame(
        std::uint32_t sessionId,
        std::uint32_t senderId,
        std::uint32_t sequence,
        std::uint16_t width,
        std::uint16_t height,
        std::vector<std::uint8_t> payload);
}

#endif // INCLUDE_MJPEG_FRAME_H
