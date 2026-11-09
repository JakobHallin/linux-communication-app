#include "audio_client_session.h"
#include "audio_packet.h"

#include "../support/check.h"
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    client::AudioClientSession session(77, 9);
    const std::vector<std::uint8_t> encoded{0x10, 0x20, 0x30};

    const auto first = session.makePacket(encoded);
    TEST_CHECK(first.has_value());
    const auto firstPacket = media::deserializeAudioPacket(*first);
    TEST_CHECK(firstPacket.has_value());
    TEST_CHECK(firstPacket->sessionId == 77);
    TEST_CHECK(firstPacket->senderId == 9);
    TEST_CHECK(firstPacket->sequence == 0);
    TEST_CHECK(firstPacket->payload == encoded);

    const auto second = session.makePacket(encoded);
    TEST_CHECK(second.has_value());
    const auto secondPacket = media::deserializeAudioPacket(*second);
    TEST_CHECK(secondPacket.has_value());
    TEST_CHECK(secondPacket->sequence == 1);

    session.setMuted(true);
    TEST_CHECK(session.muted());
    TEST_CHECK(!session.makePacket(encoded).has_value());

    session.setMuted(false);
    TEST_CHECK(!session.makePacket({}).has_value());
    TEST_CHECK(session.makePacket(encoded).has_value());

    std::cout << "Phase 5 audio client session test passed\n";
}
