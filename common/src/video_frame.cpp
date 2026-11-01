#include "video_frame.h"

#include <array>
#include <algorithm>

namespace media {
    namespace {
        constexpr std::array<std::uint8_t, 4> magic{'V', 'I', 'D', '1'};
        constexpr std::uint8_t version = 1;
        constexpr std::size_t headerBytes = 26;

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

    std::vector<std::uint8_t> serializeVideoFrame(const VideoFrame& frame) {
        if (frame.width == 0 || frame.height == 0 ||
            frame.payload.size() > maximumVideoFrameBytes) {
            return {};
        }

        std::vector<std::uint8_t> output;
        output.reserve(headerBytes + frame.payload.size());
        output.insert(output.end(), magic.begin(), magic.end());
        output.push_back(version);
        output.push_back(0);
        append32(output, frame.sessionId);
        append32(output, frame.senderId);
        append32(output, frame.sequence);
        append16(output, frame.width);
        append16(output, frame.height);
        append32(output, static_cast<std::uint32_t>(frame.payload.size()));
        output.insert(output.end(), frame.payload.begin(), frame.payload.end());
        return output;
    }

    std::optional<VideoFrame> deserializeVideoFrame(
        const std::vector<std::uint8_t>& bytes) {
        if (bytes.size() < headerBytes ||
            bytes.size() > headerBytes + maximumVideoFrameBytes ||
            !std::equal(magic.begin(), magic.end(), bytes.begin()) ||
            bytes[4] != version) {
            return std::nullopt;
        }

        const auto payloadSize = read32(bytes, 22);
        if (payloadSize > maximumVideoFrameBytes ||
            headerBytes + payloadSize != bytes.size()) {
            return std::nullopt;
        }

        const auto width = read16(bytes, 18);
        const auto height = read16(bytes, 20);
        if (width == 0 || height == 0) return std::nullopt;

        return VideoFrame{
            .sessionId = read32(bytes, 6),
            .senderId = read32(bytes, 10),
            .sequence = read32(bytes, 14),
            .width = width,
            .height = height,
            .payload = std::vector<std::uint8_t>(bytes.begin() + headerBytes,
                                                 bytes.end())
        };
    }
}
