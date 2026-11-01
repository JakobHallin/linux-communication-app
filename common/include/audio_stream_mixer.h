#ifndef INCLUDE_AUDIO_STREAM_MIXER_H
#define INCLUDE_AUDIO_STREAM_MIXER_H

#include "audio_codec.h"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace media {
    class AudioStreamMixer {
    public:
        bool submit(std::uint32_t senderId, std::span<const std::int16_t> samples);
        void setMuted(std::uint32_t senderId, bool muted);
        void remove(std::uint32_t senderId);
        std::vector<std::int16_t> mix() const;

    private:
        std::unordered_map<std::uint32_t, std::vector<std::int16_t>> streams;
        std::unordered_map<std::uint32_t, bool> muted;
    };
}

#endif // INCLUDE_AUDIO_STREAM_MIXER_H
