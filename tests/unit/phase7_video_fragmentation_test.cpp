#include "video_frame.h"

#include "../support/check.h"
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
    TEST_CHECK(datagrams.size() > 1);
    for (const auto& datagram : datagrams) {
        TEST_CHECK(datagram.size() <= media::maximumVideoDatagramBytes);
    }

    media::VideoFrameReassembler reassembler;
    std::optional<media::VideoFrame> rebuilt;
    for (auto it = datagrams.rbegin(); it != datagrams.rend(); ++it) {
        rebuilt = reassembler.accept(*it);
    }
    TEST_CHECK(rebuilt.has_value());
    TEST_CHECK(rebuilt->sessionId == original.sessionId);
    TEST_CHECK(rebuilt->senderId == original.senderId);
    TEST_CHECK(rebuilt->sequence == original.sequence);
    TEST_CHECK(rebuilt->width == original.width);
    TEST_CHECK(rebuilt->height == original.height);
    TEST_CHECK(rebuilt->payload == original.payload);

    std::cout << "Phase 7 video fragmentation test passed\n";
}
