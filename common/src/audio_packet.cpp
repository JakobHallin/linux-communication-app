#include "audio_packet.h"

#include <array>
#include <algorithm>

namespace media {
    namespace {
        constexpr std::array<std::uint8_t, 4> magic{'A', 'U', 'D', '1'};
        constexpr std::uint8_t version = 1;
        constexpr std::size_t headerBytes = 20;

        void append16(std::vector<std::uint8_t>& output, std::uint16_t value) {
            output.push_back(static_cast<std::uint8_t>(value >> 8));
            output.push_back(static_cast<std::uint8_t>(value));
        }

        void append32(std::vector<std::uint8_t>& output, std::uint32_t value) {
            output.push_back(static_cast<std::uint8_t>(value >> 24));
            output.push_back(static_cast<std::uint8_t>(value >> 16));
            output.push_back(static_cast<std::uint8_t>(value >> 8));
            output.push_back(static_cast<std::uint8_t>(value));
        }

        std::uint16_t read16(const std::vector<std::uint8_t>& bytes,
                             std::size_t offset) {
            return (static_cast<std::uint16_t>(bytes[offset]) << 8) |
                   static_cast<std::uint16_t>(bytes[offset + 1]);
        }

        std::uint32_t read32(const std::vector<std::uint8_t>& bytes,
                             std::size_t offset) {
            return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
                   (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
                   (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
                   static_cast<std::uint32_t>(bytes[offset + 3]);
        }
    }

    std::vector<std::uint8_t> serializeAudioPacket(const AudioPacket& packet) {
        if (packet.payload.size() > maximumAudioPacketBytes - headerBytes ||
            packet.payload.size() > UINT16_MAX) {
            return {};
        }

        std::vector<std::uint8_t> output;
        output.reserve(headerBytes + packet.payload.size());
        output.insert(output.end(), magic.begin(), magic.end());
        output.push_back(version);
        output.push_back(0);
        append32(output, packet.sessionId);
        append32(output, packet.senderId);
        append32(output, packet.sequence);
        append16(output, static_cast<std::uint16_t>(packet.payload.size()));
        output.insert(output.end(), packet.payload.begin(), packet.payload.end());
        return output;
    }

    std::optional<AudioPacket> deserializeAudioPacket(
        const std::vector<std::uint8_t>& bytes) {
        if (bytes.size() < headerBytes || bytes.size() > maximumAudioPacketBytes ||
            !std::equal(magic.begin(), magic.end(), bytes.begin()) ||
            bytes[4] != version) {
            return std::nullopt;
        }

        const std::size_t payloadSize = read16(bytes, 18);
        if (headerBytes + payloadSize != bytes.size()) {
            return std::nullopt;
        }

        AudioPacket packet{
            .sessionId = read32(bytes, 6),
            .senderId = read32(bytes, 10),
            .sequence = read32(bytes, 14),
            .payload = std::vector<std::uint8_t>(bytes.begin() + headerBytes,
                                                 bytes.end())
        };
        return packet;
    }
}
