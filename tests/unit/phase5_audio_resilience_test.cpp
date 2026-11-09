#include "audio_jitter_buffer.h"
#include "audio_stream_mixer.h"

#include "../support/check.h"
#include <iostream>
#include <vector>

int main() {
    const std::vector<std::int16_t> first(media::OpusCodec::frameSamples, 1000);
    const std::vector<std::int16_t> second(media::OpusCodec::frameSamples, 2000);
    media::AudioJitterBuffer jitter;

    TEST_CHECK(jitter.push(2, second));
    TEST_CHECK(jitter.push(1, first));
    TEST_CHECK(jitter.popNext()->front() == 1000);
    TEST_CHECK(jitter.popNext()->front() == 2000);

    TEST_CHECK(jitter.push(4, second));
    const auto concealed = jitter.popNext();
    TEST_CHECK(concealed.has_value());
    TEST_CHECK(concealed->front() == 0);
    TEST_CHECK(jitter.popNext()->front() == 2000);

    media::AudioStreamMixer mixer;
    TEST_CHECK(mixer.submit(1, first));
    TEST_CHECK(mixer.submit(2, second));
    mixer.setMuted(2, true);
    TEST_CHECK(mixer.mix().front() == 1000);
    mixer.setMuted(2, false);
    TEST_CHECK(mixer.mix().front() == 1500);

    std::cout << "Phase 5 audio resilience test passed\n";
}
