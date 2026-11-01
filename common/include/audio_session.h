#ifndef INCLUDE_AUDIO_SESSION_H
#define INCLUDE_AUDIO_SESSION_H

#include "audio_packet.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace media {
    class AudioSessionRegistry {
    public:
        void registerSender(std::uint32_t sessionId, std::uint32_t senderId);
        void unregisterSender(std::uint32_t sessionId, std::uint32_t senderId);
        bool accepts(const AudioPacket& packet) const;

    private:
        std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> senders;
    };
}

#endif // INCLUDE_AUDIO_SESSION_H
