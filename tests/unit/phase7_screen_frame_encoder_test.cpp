#include "screen_frame_encoder.h"
#include "mjpeg_frame.h"

#include <cassert>
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
    assert(frame.has_value());
    assert(frame->sessionId == 7);
    assert(frame->senderId == 3);
    assert(frame->sequence == 11);
    assert(frame->width == 2);
    assert(frame->height == 2);
    assert(media::isValidMjpeg(frame->payload));

    auto invalid = source;
    invalid.stride = 4;
    assert(!media::makeMjpegVideoFrameFromBgra(7, 3, 12, invalid).has_value());

    std::cout << "Phase 7 screen frame encoder test passed\n";
}
