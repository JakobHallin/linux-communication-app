#include "video_frame.h"

#include "../support/check.h"
#include <iostream>
#include <vector>

int main() {
    const media::VideoFrame original{
        .sessionId = 7,
        .senderId = 3,
        .sequence = 42,
        .width = 1280,
        .height = 720,
        .payload = {0x10, 0x20, 0x30, 0x40}
    };

    const auto encoded = media::serializeVideoFrame(original);
    TEST_CHECK(!encoded.empty());
    const auto decoded = media::deserializeVideoFrame(encoded);
    TEST_CHECK(decoded.has_value());
    TEST_CHECK(decoded->sessionId == original.sessionId);
    TEST_CHECK(decoded->senderId == original.senderId);
    TEST_CHECK(decoded->sequence == original.sequence);
    TEST_CHECK(decoded->width == original.width);
    TEST_CHECK(decoded->height == original.height);
    TEST_CHECK(decoded->payload == original.payload);

    auto invalidMagic = encoded;
    invalidMagic[0] = 'X';
    TEST_CHECK(!media::deserializeVideoFrame(invalidMagic).has_value());

    auto truncated = encoded;
    truncated.pop_back();
    TEST_CHECK(!media::deserializeVideoFrame(truncated).has_value());

    auto oversized = original;
    oversized.payload.resize(media::maximumVideoFrameBytes + 1);
    TEST_CHECK(media::serializeVideoFrame(oversized).empty());

    std::cout << "Phase 6 video frame test passed\n";
}
