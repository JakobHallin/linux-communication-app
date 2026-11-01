#include "video_capture_device.h"
#include "mjpeg_frame.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <utility>

namespace client {
    namespace {
        int ioctlRetry(int descriptor, unsigned long request, void* argument) {
            int result = 0;
            do {
                result = ioctl(descriptor, request, argument);
            } while (result < 0 && errno == EINTR);
            return result;
        }
    }

    VideoCaptureDevice::VideoCaptureDevice(std::string path)
        : devicePath(std::move(path)) {}

    VideoCaptureDevice::~VideoCaptureDevice() { stop(); }

    bool VideoCaptureDevice::start() {
        if (isRunning) return true;
        fileDescriptor = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
        if (fileDescriptor < 0) return false;

        v4l2_capability capabilities{};
        if (ioctlRetry(fileDescriptor, VIDIOC_QUERYCAP, &capabilities) < 0 ||
            !(capabilities.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
            !(capabilities.capabilities & V4L2_CAP_STREAMING) ||
            !configure()) {
            stop();
            return false;
        }

        v4l2_requestbuffers request{};
        request.count = 4;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        if (ioctlRetry(fileDescriptor, VIDIOC_REQBUFS, &request) < 0 ||
            request.count == 0) {
            stop();
            return false;
        }

        buffers.resize(request.count);
        for (std::uint32_t index = 0; index < request.count; ++index) {
            v4l2_buffer buffer{};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = index;
            if (ioctlRetry(fileDescriptor, VIDIOC_QUERYBUF, &buffer) < 0) {
                stop();
                return false;
            }
            buffers[index].length = buffer.length;
            buffers[index].address = mmap(nullptr, buffer.length,
                                           PROT_READ | PROT_WRITE, MAP_SHARED,
                                           fileDescriptor, buffer.m.offset);
            if (buffers[index].address == MAP_FAILED) {
                buffers[index].address = nullptr;
                stop();
                return false;
            }
            if (ioctlRetry(fileDescriptor, VIDIOC_QBUF, &buffer) < 0) {
                stop();
                return false;
            }
        }

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctlRetry(fileDescriptor, VIDIOC_STREAMON, &type) < 0) {
            stop();
            return false;
        }
        isRunning = true;
        return true;
    }

    void VideoCaptureDevice::stop() {
        if (fileDescriptor >= 0 && isRunning) {
            v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctlRetry(fileDescriptor, VIDIOC_STREAMOFF, &type);
        }
        isRunning = false;
        releaseBuffers();
        if (fileDescriptor >= 0) {
            ::close(fileDescriptor);
            fileDescriptor = -1;
        }
    }

    bool VideoCaptureDevice::running() const { return isRunning; }
    std::uint16_t VideoCaptureDevice::width() const { return frameWidth; }
    std::uint16_t VideoCaptureDevice::height() const { return frameHeight; }

    std::optional<std::vector<std::uint8_t>> VideoCaptureDevice::readFrame(
        int timeoutMilliseconds) {
        if (!isRunning || timeoutMilliseconds < 0) return std::nullopt;
        pollfd descriptor{fileDescriptor, POLLIN, 0};
        if (poll(&descriptor, 1, timeoutMilliseconds) <= 0) return std::nullopt;

        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        if (ioctlRetry(fileDescriptor, VIDIOC_DQBUF, &buffer) < 0 ||
            buffer.index >= buffers.size()) {
            return std::nullopt;
        }

        const auto* begin = static_cast<const std::uint8_t*>(buffers[buffer.index].address);
        std::vector<std::uint8_t> frame(begin, begin + buffer.bytesused);
        if (ioctlRetry(fileDescriptor, VIDIOC_QBUF, &buffer) < 0) return std::nullopt;
        return frame;
    }

    std::optional<media::VideoFrame> VideoCaptureDevice::readVideoFrame(
        std::uint32_t sessionId,
        std::uint32_t senderId,
        std::uint32_t sequence,
        int timeoutMilliseconds) {
        const auto payload = readFrame(timeoutMilliseconds);
        if (!payload.has_value()) return std::nullopt;

        return media::makeMjpegVideoFrame(
            sessionId, senderId, sequence, frameWidth, frameHeight, *payload);
    }

    bool VideoCaptureDevice::configure() {
        v4l2_format format{};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = 1280;
        format.fmt.pix.height = 720;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        format.fmt.pix.field = V4L2_FIELD_ANY;
        if (ioctlRetry(fileDescriptor, VIDIOC_S_FMT, &format) < 0 ||
            format.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG ||
            format.fmt.pix.width == 0 || format.fmt.pix.height == 0) {
            return false;
        }
        frameWidth = static_cast<std::uint16_t>(format.fmt.pix.width);
        frameHeight = static_cast<std::uint16_t>(format.fmt.pix.height);
        return true;
    }

    void VideoCaptureDevice::releaseBuffers() {
        for (const auto& buffer : buffers) {
            if (buffer.address != nullptr) munmap(buffer.address, buffer.length);
        }
        buffers.clear();
    }
}
