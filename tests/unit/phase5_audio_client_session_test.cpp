#include "audio_client_session.h"
#include "audio_packet.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    client::AudioClientSession session(77, 9);
    const std::vector<std::uint8_t> encoded{0x10, 0x20, 0x30};

    const auto first = session.makePacket(encoded);
    assert(first.has_value());
    const auto firstPacket = media::deserializeAudioPacket(*first);
    assert(firstPacket.has_value());
    assert(firstPacket->sessionId == 77);
    assert(firstPacket->senderId == 9);
    assert(firstPacket->sequence == 0);
    assert(firstPacket->payload == encoded);

    const auto second = session.makePacket(encoded);
    assert(second.has_value());
    const auto secondPacket = media::deserializeAudioPacket(*second);
    assert(secondPacket.has_value());
    assert(secondPacket->sequence == 1);

    session.setMuted(true);
    assert(session.muted());
    assert(!session.makePacket(encoded).has_value());

    session.setMuted(false);
    assert(!session.makePacket({}).has_value());
    assert(session.makePacket(encoded).has_value());

    std::cout << "Phase 5 audio client session test passed\n";
}
