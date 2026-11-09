#ifndef INCLUDE_SCREEN_CAPTURE_SOURCE_H
#define INCLUDE_SCREEN_CAPTURE_SOURCE_H

#include "video_frame.h"

#include <cstdint>
#include <optional>

namespace client {
    class ScreenCaptureSource {
    public:
        virtual ~ScreenCaptureSource() = default;
        virtual bool start() = 0;
        virtual void stop() = 0;
        virtual bool running() const = 0;
        virtual std::optional<media::VideoFrame> readFrame(
            std::uint32_t sessionId,
            std::uint32_t senderId,
            std::uint32_t sequence,
            int timeoutMilliseconds) = 0;
    };
}

#endif // INCLUDE_SCREEN_CAPTURE_SOURCE_H
