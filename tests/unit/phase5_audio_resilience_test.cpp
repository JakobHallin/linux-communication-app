#include "audio_jitter_buffer.h"
#include "audio_stream_mixer.h"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    const std::vector<std::int16_t> first(media::OpusCodec::frameSamples, 1000);
    const std::vector<std::int16_t> second(media::OpusCodec::frameSamples, 2000);
    media::AudioJitterBuffer jitter;

    assert(jitter.push(2, second));
    assert(jitter.push(1, first));
    assert(jitter.popNext()->front() == 1000);
    assert(jitter.popNext()->front() == 2000);

    assert(jitter.push(4, second));
    const auto concealed = jitter.popNext();
    assert(concealed.has_value());
    assert(concealed->front() == 0);
    assert(jitter.popNext()->front() == 2000);

    media::AudioStreamMixer mixer;
    assert(mixer.submit(1, first));
    assert(mixer.submit(2, second));
    mixer.setMuted(2, true);
    assert(mixer.mix().front() == 1000);
    mixer.setMuted(2, false);
    assert(mixer.mix().front() == 1500);

    std::cout << "Phase 5 audio resilience test passed\n";
}
