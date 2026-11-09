#include "video_frame.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <limits>

namespace media {
    namespace {
        constexpr std::array<std::uint8_t, 4> magic{'V', 'I', 'D', '1'};
        constexpr std::array<std::uint8_t, 4> fragmentMagic{'V', 'F', 'R', '1'};
        constexpr std::uint8_t version = 1;
        constexpr std::size_t headerBytes = 26;
        constexpr std::size_t fragmentHeaderBytes = 30;
        constexpr std::size_t fragmentPayloadBytes = maximumVideoDatagramBytes -
                                                       fragmentHeaderBytes;

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

        std::uint16_t read16At(const std::vector<std::uint8_t>& bytes,
                               std::size_t offset) {
            return read16(bytes, offset);
        }

        std::uint32_t read32At(const std::vector<std::uint8_t>& bytes,
                               std::size_t offset) {
            return read32(bytes, offset);
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

    std::vector<std::vector<std::uint8_t>> serializeVideoFrameDatagrams(
        const VideoFrame& frame) {
        const auto complete = serializeVideoFrame(frame);
        if (complete.empty()) return {};
        if (complete.size() <= maximumVideoDatagramBytes) return {complete};

        const auto count = (frame.payload.size() + fragmentPayloadBytes - 1) /
                           fragmentPayloadBytes;
        if (count > std::numeric_limits<std::uint16_t>::max()) return {};

        std::vector<std::vector<std::uint8_t>> datagrams;
        datagrams.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto offset = index * fragmentPayloadBytes;
            const auto length = std::min(fragmentPayloadBytes,
                                         frame.payload.size() - offset);
            std::vector<std::uint8_t> datagram;
            datagram.reserve(fragmentHeaderBytes + length);
            datagram.insert(datagram.end(), fragmentMagic.begin(), fragmentMagic.end());
            datagram.push_back(version);
            datagram.push_back(0);
            append32(datagram, frame.sessionId);
            append32(datagram, frame.senderId);
            append32(datagram, frame.sequence);
            append16(datagram, frame.width);
            append16(datagram, frame.height);
            append16(datagram, static_cast<std::uint16_t>(index));
            append16(datagram, static_cast<std::uint16_t>(count));
            append32(datagram, static_cast<std::uint32_t>(frame.payload.size()));
            datagram.insert(datagram.end(), frame.payload.begin() + offset,
                            frame.payload.begin() + offset + length);
            datagrams.push_back(std::move(datagram));
        }
        return datagrams;
    }

    std::size_t VideoFrameReassembler::KeyHash::operator()(const Key& key) const {
        std::size_t hash = key.sessionId;
        hash = hash * 31 + key.senderId;
        return hash * 31 + key.sequence;
    }

    std::optional<VideoFrame> VideoFrameReassembler::accept(
        const std::vector<std::uint8_t>& datagram) {
        if (const auto complete = deserializeVideoFrame(datagram); complete.has_value()) {
            return complete;
        }
        if (datagram.size() < fragmentHeaderBytes ||
            !std::equal(fragmentMagic.begin(), fragmentMagic.end(), datagram.begin()) ||
            datagram[4] != version) {
            return std::nullopt;
        }

        const auto sessionId = read32At(datagram, 6);
        const auto senderId = read32At(datagram, 10);
        const auto sequence = read32At(datagram, 14);
        const auto width = read16At(datagram, 18);
        const auto height = read16At(datagram, 20);
        const auto index = read16At(datagram, 22);
        const auto count = read16At(datagram, 24);
        const auto totalBytes = read32At(datagram, 26);
        if (width == 0 || height == 0 || count == 0 || index >= count ||
            totalBytes == 0 || totalBytes > maximumVideoFrameBytes ||
            datagram.size() - fragmentHeaderBytes > fragmentPayloadBytes) {
            return std::nullopt;
        }

        const auto now = std::chrono::steady_clock::now();
        for (auto it = pending.begin(); it != pending.end();) {
            if (now - it->second.lastReceived > std::chrono::seconds(2)) {
                it = pending.erase(it);
            } else {
                ++it;
            }
        }

        const Key key{sessionId, senderId, sequence};
        auto [it, inserted] = pending.try_emplace(key, Pending{
            width, height, totalBytes, std::vector<std::vector<std::uint8_t>>(count),
            0, now});
        auto& frame = it->second;
        if (!inserted && (frame.width != width || frame.height != height ||
                          frame.totalBytes != totalBytes || frame.parts.size() != count)) {
            pending.erase(it);
            return std::nullopt;
        }
        frame.lastReceived = now;
        if (frame.parts[index].empty()) {
            frame.parts[index] = std::vector<std::uint8_t>(
                datagram.begin() + fragmentHeaderBytes, datagram.end());
            ++frame.receivedParts;
        }
        if (frame.receivedParts != frame.parts.size()) return std::nullopt;

        std::vector<std::uint8_t> payload;
        payload.reserve(frame.totalBytes);
        for (const auto& part : frame.parts) {
            payload.insert(payload.end(), part.begin(), part.end());
        }
        if (payload.size() != frame.totalBytes) {
            pending.erase(it);
            return std::nullopt;
        }
        VideoFrame result{sessionId, senderId, sequence, width, height,
                          std::move(payload)};
        pending.erase(it);
        return result;
    }
}
