#include "screen_frame_encoder.h"
#include "mjpeg_frame.h"

#include "../support/check.h"
#include <iostream>

int main() {
    const media::BgraFrame source{
        .width = 2,
        .height = 2,
        .stride = 8,
        .pixels = {
            0, 0, 255, 255,   0, 255, 0, 255,
            255, 0, 0, 255,   255, 255, 255, 255
        }
    };

    const auto frame = media::makeMjpegVideoFrameFromBgra(7, 3, 11, source);
    TEST_CHECK(frame.has_value());
    TEST_CHECK(frame->sessionId == 7);
    TEST_CHECK(frame->senderId == 3);
    TEST_CHECK(frame->sequence == 11);
    TEST_CHECK(frame->width == 2);
    TEST_CHECK(frame->height == 2);
    TEST_CHECK(media::isValidMjpeg(frame->payload));

    auto invalid = source;
    invalid.stride = 4;
    TEST_CHECK(!media::makeMjpegVideoFrameFromBgra(7, 3, 12, invalid).has_value());

    std::cout << "Phase 7 screen frame encoder test passed\n";
}
