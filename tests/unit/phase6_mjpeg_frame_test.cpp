#include "mjpeg_frame.h"

#include "../support/check.h"
#include <iostream>

int main() {
    const std::vector<std::uint8_t> sample{0xff, 0xd8, 0x01, 0x02, 0xff, 0xd9};
    TEST_CHECK(media::isValidMjpeg(sample));

    const auto frame = media::makeMjpegVideoFrame(
        9, 4, 12, 1280, 720, sample);
    TEST_CHECK(frame.has_value());
    TEST_CHECK(frame->sessionId == 9);
    TEST_CHECK(frame->senderId == 4);
    TEST_CHECK(frame->sequence == 12);
    TEST_CHECK(frame->width == 1280);
    TEST_CHECK(frame->height == 720);
    TEST_CHECK(frame->payload == sample);

    auto missingStart = sample;
    missingStart[0] = 0;
    TEST_CHECK(!media::isValidMjpeg(missingStart));

    auto missingEnd = sample;
    missingEnd.back() = 0;
    TEST_CHECK(!media::isValidMjpeg(missingEnd));

    TEST_CHECK(!media::makeMjpegVideoFrame(
        9, 4, 13, 0, 720, sample).has_value());
    TEST_CHECK(!media::makeMjpegVideoFrame(
        9, 4, 14, 1280, 720, {0x01, 0x02}).has_value());

    std::cout << "Phase 6 MJPEG frame test passed\n";
}
