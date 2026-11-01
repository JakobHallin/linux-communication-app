#ifndef INCLUDE_AUDIO_CODEC_H
#define INCLUDE_AUDIO_CODEC_H

#include "audio_packet.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct OpusDecoder;
struct OpusEncoder;

namespace media {
    class OpusCodec {
    public:
        static constexpr std::size_t sampleRate = 48'000;
        static constexpr std::size_t frameSamples = 960;

        OpusCodec();
        ~OpusCodec();
        OpusCodec(const OpusCodec&) = delete;
        OpusCodec& operator=(const OpusCodec&) = delete;

        std::vector<std::uint8_t> encode(std::span<const std::int16_t> samples);
        std::optional<std::vector<std::int16_t>> decode(
            std::span<const std::uint8_t> packet);

    private:
        OpusEncoder* encoder = nullptr;
        OpusDecoder* decoder = nullptr;
    };
}

#endif // INCLUDE_AUDIO_CODEC_H
