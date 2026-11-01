#ifndef INCLUDE_VOICE_CLIENT_H
#define INCLUDE_VOICE_CLIENT_H

#include <cstdint>
#include <string>

namespace client {
    class VoiceClient {
    public:
        VoiceClient(std::string serverHost, std::uint16_t mediaPort,
                    std::uint32_t sessionId, std::uint32_t senderId,
                    std::string captureDevice, std::string playbackDevice);
        ~VoiceClient();
        VoiceClient(const VoiceClient&) = delete;
        VoiceClient& operator=(const VoiceClient&) = delete;

        bool start();
        void activate();
        void stop();
        void setMuted(bool muted);
        std::uint16_t localPort() const;
        void setMediaPort(std::uint16_t port);
        void setIdentity(std::uint32_t sessionId, std::uint32_t senderId);

    private:
        void captureLoop();
        void playbackLoop();

        std::string serverHost;
        std::uint16_t mediaPort;
        std::uint32_t sessionId;
        std::uint32_t senderId;
        std::string captureDevice;
        std::string playbackDevice;
        int socketFd = -1;
        std::uint16_t boundPort = 0;
        bool running = false;
        struct Impl;
        Impl* impl = nullptr;
    };
}

#endif // INCLUDE_VOICE_CLIENT_H
