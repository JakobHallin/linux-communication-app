#ifndef INCLUDE_AUDIO_PACKET_H
#define INCLUDE_AUDIO_PACKET_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace media {
    constexpr std::size_t maximumAudioPacketBytes = 1200;

    struct AudioPacket {
        std::uint32_t sessionId;
        std::uint32_t senderId;
        std::uint32_t sequence;
        std::vector<std::uint8_t> payload;
    };

    std::vector<std::uint8_t> serializeAudioPacket(const AudioPacket& packet);
    std::optional<AudioPacket> deserializeAudioPacket(
        const std::vector<std::uint8_t>& bytes);
}

#endif // INCLUDE_AUDIO_PACKET_H
