#include "audio_stream_mixer.h"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    media::AudioStreamMixer mixer;
    const std::vector<std::int16_t> first(media::OpusCodec::frameSamples, 1000);
    const std::vector<std::int16_t> second(media::OpusCodec::frameSamples, -500);
    const std::vector<std::int16_t> invalid(10, 1);

    assert(mixer.submit(1, first));
    assert(mixer.submit(2, second));
    assert(!mixer.submit(3, invalid));

    const auto mixed = mixer.mix();
    assert(mixed.size() == media::OpusCodec::frameSamples);
    assert(mixed.front() == 250);
    assert(mixed.back() == 250);

    mixer.remove(2);
    const auto single = mixer.mix();
    assert(single.front() == 1000);
    assert(single.back() == 1000);

    mixer.remove(1);
    const auto silent = mixer.mix();
    assert(silent.front() == 0);
    assert(silent.back() == 0);

    std::cout << "Phase 5 multi-stream mixer test passed\n";
}
