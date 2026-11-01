#ifndef INCLUDE_AUDIO_JITTER_BUFFER_H
#define INCLUDE_AUDIO_JITTER_BUFFER_H

#include "audio_codec.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace media {
    class AudioJitterBuffer {
    public:
        bool push(std::uint32_t sequence, std::span<const std::int16_t> samples);
        std::optional<std::vector<std::int16_t>> popNext();

    private:
        std::map<std::uint32_t, std::vector<std::int16_t>> frames;
        std::optional<std::uint32_t> nextSequence;
    };
}

#endif // INCLUDE_AUDIO_JITTER_BUFFER_H
