#include "audio_client_session.h"

namespace client {
    AudioClientSession::AudioClientSession(std::uint32_t session,
                                           std::uint32_t sender)
        : sessionId(session), senderId(sender) {}

    void AudioClientSession::setMuted(bool muted) { isMuted = muted; }

    bool AudioClientSession::muted() const { return isMuted; }

    std::optional<std::vector<std::uint8_t>> AudioClientSession::makePacket(
        std::span<const std::uint8_t> encodedAudio) {
        if (isMuted || encodedAudio.empty()) return std::nullopt;

        const media::AudioPacket packet{
            .sessionId = sessionId,
            .senderId = senderId,
            .sequence = nextSequence++,
            .payload = std::vector<std::uint8_t>(encodedAudio.begin(), encodedAudio.end())
        };
        return media::serializeAudioPacket(packet);
    }
}
