#ifndef INCLUDE_VIDEO_CAPTURE_DEVICE_H
#define INCLUDE_VIDEO_CAPTURE_DEVICE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "video_frame.h"

namespace client {
    class VideoCaptureDevice {
    public:
        explicit VideoCaptureDevice(std::string devicePath);
        ~VideoCaptureDevice();
        VideoCaptureDevice(const VideoCaptureDevice&) = delete;
        VideoCaptureDevice& operator=(const VideoCaptureDevice&) = delete;

        bool start();
        void stop();
        bool running() const;
        std::uint16_t width() const;
        std::uint16_t height() const;
        std::optional<std::vector<std::uint8_t>> readFrame(int timeoutMilliseconds);
        std::optional<media::VideoFrame> readVideoFrame(
            std::uint32_t sessionId,
            std::uint32_t senderId,
            std::uint32_t sequence,
            int timeoutMilliseconds);

    private:
        struct Buffer {
            void* address = nullptr;
            std::size_t length = 0;
        };

        bool configure();
        void releaseBuffers();

        std::string devicePath;
        int fileDescriptor = -1;
        std::vector<Buffer> buffers;
        std::uint16_t frameWidth = 0;
        std::uint16_t frameHeight = 0;
        bool isRunning = false;
    };
}

#endif // INCLUDE_VIDEO_CAPTURE_DEVICE_H
