#include "mjpeg_frame.h"

#include <cassert>
#include <iostream>

int main() {
    const std::vector<std::uint8_t> sample{0xff, 0xd8, 0x01, 0x02, 0xff, 0xd9};
    assert(media::isValidMjpeg(sample));

    const auto frame = media::makeMjpegVideoFrame(
        9, 4, 12, 1280, 720, sample);
    assert(frame.has_value());
    assert(frame->sessionId == 9);
    assert(frame->senderId == 4);
    assert(frame->sequence == 12);
    assert(frame->width == 1280);
    assert(frame->height == 720);
    assert(frame->payload == sample);

    auto missingStart = sample;
    missingStart[0] = 0;
    assert(!media::isValidMjpeg(missingStart));

    auto missingEnd = sample;
    missingEnd.back() = 0;
    assert(!media::isValidMjpeg(missingEnd));

    assert(!media::makeMjpegVideoFrame(
        9, 4, 13, 0, 720, sample).has_value());
    assert(!media::makeMjpegVideoFrame(
        9, 4, 14, 1280, 720, {0x01, 0x02}).has_value());

    std::cout << "Phase 6 MJPEG frame test passed\n";
}
