#include "video_frame.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

int main() {
    media::VideoFrame original{
        .sessionId = 11,
        .senderId = 4,
        .sequence = 91,
        .width = 1920,
        .height = 1080,
        .payload = std::vector<std::uint8_t>(200'000, 0x5a)
    };

    const auto datagrams = media::serializeVideoFrameDatagrams(original);
    assert(datagrams.size() > 1);
    for (const auto& datagram : datagrams) {
        assert(datagram.size() <= media::maximumVideoDatagramBytes);
    }

    media::VideoFrameReassembler reassembler;
    std::optional<media::VideoFrame> rebuilt;
    for (auto it = datagrams.rbegin(); it != datagrams.rend(); ++it) {
        rebuilt = reassembler.accept(*it);
    }
    assert(rebuilt.has_value());
    assert(rebuilt->sessionId == original.sessionId);
    assert(rebuilt->senderId == original.senderId);
    assert(rebuilt->sequence == original.sequence);
    assert(rebuilt->width == original.width);
    assert(rebuilt->height == original.height);
    assert(rebuilt->payload == original.payload);

    std::cout << "Phase 7 video fragmentation test passed\n";
}
