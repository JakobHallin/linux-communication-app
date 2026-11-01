#include "audio_session.h"

namespace media {
    void AudioSessionRegistry::registerSender(std::uint32_t sessionId,
                                               std::uint32_t senderId) {
        senders[sessionId].insert(senderId);
    }

    void AudioSessionRegistry::unregisterSender(std::uint32_t sessionId,
                                                 std::uint32_t senderId) {
        const auto session = senders.find(sessionId);
        if (session == senders.end()) {
            return;
        }

        session->second.erase(senderId);
        if (session->second.empty()) {
            senders.erase(session);
        }
    }

    bool AudioSessionRegistry::accepts(const AudioPacket& packet) const {
        const auto session = senders.find(packet.sessionId);
        return session != senders.end() &&
               session->second.contains(packet.senderId);
    }
}
