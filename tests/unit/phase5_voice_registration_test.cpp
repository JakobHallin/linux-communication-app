#include "chat_protocol.h"

#include "../support/check.h"
#include <iostream>

int main() {
    TEST_CHECK(http::parseVoicePort("VOICE_PORT 9001") == 9001);
    TEST_CHECK(http::parseVoicePort("VOICE_PORT 1") == 1);
    TEST_CHECK(http::parseVoicePort("VOICE_PORT 65535") == 65535);
    TEST_CHECK(!http::parseVoicePort("VOICE_PORT 0").has_value());
    TEST_CHECK(!http::parseVoicePort("VOICE_PORT 65536").has_value());
    TEST_CHECK(!http::parseVoicePort("VOICE_PORT nope").has_value());
    TEST_CHECK(!http::parseVoicePort("VOICE_PORT 9001 extra").has_value());
    TEST_CHECK(!http::parseVoicePort("MSG VOICE_PORT 9001").has_value());
    std::cout << "Phase 5 voice registration test passed\n";
}
