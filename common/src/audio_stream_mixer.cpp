#include "audio_stream_mixer.h"

#include <algorithm>
#include <cstdint>

namespace media {
    bool AudioStreamMixer::submit(std::uint32_t senderId,
                                  std::span<const std::int16_t> samples) {
        if (samples.size() != OpusCodec::frameSamples) {
            return false;
        }
        streams[senderId] = std::vector<std::int16_t>(samples.begin(), samples.end());
        return true;
    }

    void AudioStreamMixer::remove(std::uint32_t senderId) {
        streams.erase(senderId);
    }

    std::vector<std::int16_t> AudioStreamMixer::mix() const {
        std::vector<std::int16_t> output(OpusCodec::frameSamples, 0);
        if (streams.empty()) {
            return output;
        }

        for (std::size_t sample = 0; sample < output.size(); ++sample) {
            std::int64_t sum = 0;
            for (const auto& [senderId, stream] : streams) {
                (void)senderId;
                sum += stream[sample];
            }
            const auto mixed = sum / static_cast<std::int64_t>(streams.size());
            output[sample] = static_cast<std::int16_t>(std::clamp<std::int64_t>(
                mixed, INT16_MIN, INT16_MAX));
        }
        return output;
    }
}
