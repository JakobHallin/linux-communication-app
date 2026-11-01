#ifndef INCLUDE_AUDIO_CLIENT_SESSION_H
#define INCLUDE_AUDIO_CLIENT_SESSION_H

#include "audio_packet.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace client {
    class AudioClientSession {
    public:
        AudioClientSession(std::uint32_t sessionId, std::uint32_t senderId);

        void setMuted(bool muted);
        bool muted() const;
        std::optional<std::vector<std::uint8_t>> makePacket(
            std::span<const std::uint8_t> encodedAudio);

    private:
        std::uint32_t sessionId;
        std::uint32_t senderId;
        std::uint32_t nextSequence = 0;
        bool isMuted = false;
    };
}

#endif // INCLUDE_AUDIO_CLIENT_SESSION_H
