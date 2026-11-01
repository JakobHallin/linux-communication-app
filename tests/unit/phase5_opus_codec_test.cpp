#include "audio_codec.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    media::OpusCodec codec;
    std::vector<std::int16_t> samples(media::OpusCodec::frameSamples);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const double phase = 2.0 * std::acos(-1.0) * 440.0 *
                             static_cast<double>(index) / 48'000.0;
        samples[index] = static_cast<std::int16_t>(12'000.0 * std::sin(phase));
    }

    const auto encoded = codec.encode(samples);
    if (encoded.empty() || encoded.size() > media::maximumAudioPacketBytes) {
        std::cerr << "Synthetic audio was not encoded within the packet limit\n";
        return 1;
    }

    const auto decoded = codec.decode(encoded);
    if (!decoded || decoded->size() != samples.size()) {
        std::cerr << "Synthetic audio did not decode to one frame\n";
        return 1;
    }

    double inputEnergy = 0.0;
    double decodedEnergy = 0.0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        inputEnergy += std::abs(static_cast<double>(samples[index]));
        decodedEnergy += std::abs(static_cast<double>((*decoded)[index]));
    }
    if (decodedEnergy < inputEnergy * 0.25 || decodedEnergy > inputEnergy * 1.5) {
        std::cerr << "Opus round-trip energy changed unexpectedly\n";
        return 1;
    }

    std::cout << "Phase 5 Opus codec test passed\n";
    return 0;
}
