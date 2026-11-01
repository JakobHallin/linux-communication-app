#include "video_frame.h"

#include <cassert>
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
    assert(!encoded.empty());
    const auto decoded = media::deserializeVideoFrame(encoded);
    assert(decoded.has_value());
    assert(decoded->sessionId == original.sessionId);
    assert(decoded->senderId == original.senderId);
    assert(decoded->sequence == original.sequence);
    assert(decoded->width == original.width);
    assert(decoded->height == original.height);
    assert(decoded->payload == original.payload);

    auto invalidMagic = encoded;
    invalidMagic[0] = 'X';
    assert(!media::deserializeVideoFrame(invalidMagic).has_value());

    auto truncated = encoded;
    truncated.pop_back();
    assert(!media::deserializeVideoFrame(truncated).has_value());

    auto oversized = original;
    oversized.payload.resize(media::maximumVideoFrameBytes + 1);
    assert(media::serializeVideoFrame(oversized).empty());

    std::cout << "Phase 6 video frame test passed\n";
}
