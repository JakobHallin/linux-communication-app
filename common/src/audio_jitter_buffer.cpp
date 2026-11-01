#include "audio_jitter_buffer.h"

namespace media {
    bool AudioJitterBuffer::push(std::uint32_t sequence,
                                 std::span<const std::int16_t> samples) {
        if (samples.size() != OpusCodec::frameSamples ||
            (nextSequence.has_value() && sequence < *nextSequence)) {
            return false;
        }
        frames[sequence] = std::vector<std::int16_t>(samples.begin(), samples.end());
        return true;
    }

    std::optional<std::vector<std::int16_t>> AudioJitterBuffer::popNext() {
        if (frames.empty()) return std::nullopt;
        if (!nextSequence.has_value()) nextSequence = frames.begin()->first;

        const auto frame = frames.find(*nextSequence);
        ++*nextSequence;
        if (frame == frames.end()) {
            return std::vector<std::int16_t>(OpusCodec::frameSamples, 0);
        }

        auto samples = std::move(frame->second);
        frames.erase(frame);
        return samples;
    }
}
