#ifndef INCLUDE_VIDEO_CLIENT_H
#define INCLUDE_VIDEO_CLIENT_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace client {
    class VideoClient {
    public:
        VideoClient(std::string serverHost,
                    std::string captureDevice);
        ~VideoClient();
        VideoClient(const VideoClient&) = delete;
        VideoClient& operator=(const VideoClient&) = delete;

        bool start();
        bool activate();
        void stop();
        std::uint16_t localPort() const;
        void setMediaPort(std::uint16_t port);
        void setIdentity(std::uint32_t sessionId, std::uint32_t senderId);

    private:
        void captureLoop();

        std::string serverHost;
        std::string captureDevice;
        std::uint16_t mediaPort = 0;
        std::uint32_t sessionId = 0;
        std::uint32_t senderId = 0;
        int socketFd = -1;
        std::uint16_t boundPort = 0;
        std::atomic_bool running{false};
        std::thread captureThread;
        struct Impl;
        Impl* impl = nullptr;
    };
}

#endif // INCLUDE_VIDEO_CLIENT_H
