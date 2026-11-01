#include "audio_packet.h"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    media::AudioPacket original{
        .sessionId = 7,
        .senderId = 42,
        .sequence = 19,
        .payload = {0x01, 0x7f, 0x80, 0xff}
    };

    const auto encoded = media::serializeAudioPacket(original);
    if (encoded.size() > media::maximumAudioPacketBytes) {
        std::cerr << "Audio packet exceeds the UDP-safe size limit\n";
        return 1;
    }

    const auto decoded = media::deserializeAudioPacket(encoded);
    if (!decoded || decoded->sessionId != original.sessionId ||
        decoded->senderId != original.senderId ||
        decoded->sequence != original.sequence ||
        decoded->payload != original.payload) {
        std::cerr << "Audio packet did not round-trip correctly\n";
        return 1;
    }

    auto invalid = encoded;
    invalid[0] ^= 0xff;
    if (media::deserializeAudioPacket(invalid)) {
        std::cerr << "Invalid audio packet magic was accepted\n";
        return 1;
    }

    std::cout << "Phase 5 audio packet test passed\n";
    return 0;
}
