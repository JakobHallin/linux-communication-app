#include "audio_session.h"

#include "../support/check.h"
#include <iostream>

int main() {
    media::AudioSessionRegistry registry;
    const media::AudioPacket packet{
        .sessionId = 42,
        .senderId = 7,
        .sequence = 1,
        .payload = {0x01, 0x02, 0x03}
    };

    TEST_CHECK(!registry.accepts(packet));

    registry.registerSender(packet.sessionId, packet.senderId);
    TEST_CHECK(registry.accepts(packet));

    const media::AudioPacket wrongSender{
        .sessionId = packet.sessionId,
        .senderId = 8,
        .sequence = packet.sequence,
        .payload = packet.payload
    };
    TEST_CHECK(!registry.accepts(wrongSender));

    const media::AudioPacket wrongSession{
        .sessionId = 99,
        .senderId = packet.senderId,
        .sequence = packet.sequence,
        .payload = packet.payload
    };
    TEST_CHECK(!registry.accepts(wrongSession));

    registry.unregisterSender(packet.sessionId, packet.senderId);
    TEST_CHECK(!registry.accepts(packet));

    std::cout << "Phase 5 audio session authorization test passed\n";
}
