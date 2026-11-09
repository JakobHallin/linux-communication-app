#include "audio_stream_mixer.h"

#include "../support/check.h"
#include <iostream>
#include <vector>

int main() {
    media::AudioStreamMixer mixer;
    const std::vector<std::int16_t> first(media::OpusCodec::frameSamples, 1000);
    const std::vector<std::int16_t> second(media::OpusCodec::frameSamples, -500);
    const std::vector<std::int16_t> invalid(10, 1);

    TEST_CHECK(mixer.submit(1, first));
    TEST_CHECK(mixer.submit(2, second));
    TEST_CHECK(!mixer.submit(3, invalid));

    const auto mixed = mixer.mix();
    TEST_CHECK(mixed.size() == media::OpusCodec::frameSamples);
    TEST_CHECK(mixed.front() == 250);
    TEST_CHECK(mixed.back() == 250);

    mixer.remove(2);
    const auto single = mixer.mix();
    TEST_CHECK(single.front() == 1000);
    TEST_CHECK(single.back() == 1000);

    mixer.remove(1);
    const auto silent = mixer.mix();
    TEST_CHECK(silent.front() == 0);
    TEST_CHECK(silent.back() == 0);

    std::cout << "Phase 5 multi-stream mixer test passed\n";
}
