#include "chat_protocol.h"

#include <cassert>
#include <iostream>

int main() {
    assert(http::parseVoicePort("VOICE_PORT 9001") == 9001);
    assert(http::parseVoicePort("VOICE_PORT 1") == 1);
    assert(http::parseVoicePort("VOICE_PORT 65535") == 65535);
    assert(!http::parseVoicePort("VOICE_PORT 0").has_value());
    assert(!http::parseVoicePort("VOICE_PORT 65536").has_value());
    assert(!http::parseVoicePort("VOICE_PORT nope").has_value());
    assert(!http::parseVoicePort("VOICE_PORT 9001 extra").has_value());
    assert(!http::parseVoicePort("MSG VOICE_PORT 9001").has_value());
    std::cout << "Phase 5 voice registration test passed\n";
}
