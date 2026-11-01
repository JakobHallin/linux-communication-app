#ifndef INCLUDE_AUDIO_UDP_TRANSPORT_H
#define INCLUDE_AUDIO_UDP_TRANSPORT_H

#include "audio_packet.h"
#include "audio_session.h"

#include <cstdint>
#include <optional>

namespace media {
    class AudioUdpTransport {
    public:
        explicit AudioUdpTransport(std::uint16_t port = 0);
        ~AudioUdpTransport();
        AudioUdpTransport(const AudioUdpTransport&) = delete;
        AudioUdpTransport& operator=(const AudioUdpTransport&) = delete;

        bool start();
        void close();
        std::uint16_t port() const;
        void registerSender(std::uint32_t sessionId, std::uint32_t senderId);
        void unregisterSender(std::uint32_t sessionId, std::uint32_t senderId);
        std::optional<AudioPacket> receive(int timeoutMilliseconds);

    private:
        std::uint16_t requestedPort;
        int socketFd = -1;
        std::uint16_t boundPort = 0;
        AudioSessionRegistry sessions;
    };
}

#endif // INCLUDE_AUDIO_UDP_TRANSPORT_H
