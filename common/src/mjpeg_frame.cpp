#include "mjpeg_frame.h"

#include <utility>

namespace media {
    bool isValidMjpeg(const std::vector<std::uint8_t>& payload) {
        if (payload.size() < 4 || payload.size() > maximumVideoFrameBytes) {
            return false;
        }

        const bool startsWithJpegMarker = payload[0] == 0xff && payload[1] == 0xd8;
        const std::size_t end = payload.size();
        const bool endsWithJpegMarker = payload[end - 2] == 0xff && payload[end - 1] == 0xd9;
        return startsWithJpegMarker && endsWithJpegMarker;
    }

    std::optional<VideoFrame> makeMjpegVideoFrame(
        std::uint32_t sessionId,
        std::uint32_t senderId,
        std::uint32_t sequence,
        std::uint16_t width,
        std::uint16_t height,
        std::vector<std::uint8_t> payload) {
        if (width == 0 || height == 0 || !isValidMjpeg(payload)) {
            return std::nullopt;
        }

        return VideoFrame{
            .sessionId = sessionId,
            .senderId = senderId,
            .sequence = sequence,
            .width = width,
            .height = height,
            .payload = std::move(payload)
        };
    }
}
