#ifndef INCLUDE_SCREEN_VIDEO_CLIENT_H
#define INCLUDE_SCREEN_VIDEO_CLIENT_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace client {
    class ScreenVideoClient {
    public:
        explicit ScreenVideoClient(std::string serverHost);
        ~ScreenVideoClient();
        ScreenVideoClient(const ScreenVideoClient&) = delete;
        ScreenVideoClient& operator=(const ScreenVideoClient&) = delete;

        bool start();
        bool activate();
        void stop();
        std::uint16_t localPort() const;
        void setMediaPort(std::uint16_t port);
        void setIdentity(std::uint32_t sessionId, std::uint32_t senderId);

    private:
        void captureLoop();

        std::string serverHost;
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

#endif // INCLUDE_SCREEN_VIDEO_CLIENT_H
