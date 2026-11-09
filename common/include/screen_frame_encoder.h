#ifndef INCLUDE_SCREEN_FRAME_ENCODER_H
#define INCLUDE_SCREEN_FRAME_ENCODER_H

#include "video_frame.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace media {
    struct BgraFrame {
        std::uint16_t width = 0;
        std::uint16_t height = 0;
        std::int32_t stride = 0;
        std::vector<std::uint8_t> pixels;
    };

    std::optional<VideoFrame> makeMjpegVideoFrameFromBgra(
        std::uint32_t sessionId,
        std::uint32_t senderId,
        std::uint32_t sequence,
        const BgraFrame& frame,
        int quality = 75);
}

#endif // INCLUDE_SCREEN_FRAME_ENCODER_H
