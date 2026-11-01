#ifndef INCLUDE_TEXT_CLIENT_H
#define INCLUDE_TEXT_CLIENT_H

#include <cstdint>
#include <optional>
#include <string>

namespace client {
    struct VoiceReady {
        std::uint32_t sessionId;
        std::uint32_t senderId;
        std::uint16_t mediaPort;
    };

    struct VideoReady {
        std::uint32_t sessionId;
        std::uint32_t senderId;
        std::uint16_t mediaPort;
    };

    class TextClient {
    public:
        TextClient(std::string host, std::uint16_t port);
        ~TextClient();
        TextClient(const TextClient&) = delete;
        TextClient& operator=(const TextClient&) = delete;

        bool connectToServer();
        bool sendName(const std::string& name);
        bool sendVoicePort(std::uint16_t port);
        std::optional<VoiceReady> registerVoice(std::uint16_t port,
                                                int timeoutMilliseconds);
        bool sendVideoPort(std::uint16_t port);
        std::optional<VideoReady> registerVideo(std::uint16_t port,
                                                int timeoutMilliseconds);
        bool sendMessage(const std::string& message);
        std::optional<std::string> receiveLine(int timeoutMilliseconds);
        void close();

    private:
        bool sendLine(const std::string& line);

        std::string host;
        std::uint16_t port;
        int socketFd = -1;
    };
}

#endif // INCLUDE_TEXT_CLIENT_H
