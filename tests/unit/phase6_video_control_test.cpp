#include "chat_protocol.h"

#include "../support/check.h"
#include <iostream>

int main() {
    TEST_CHECK(http::parseVideoPort("VIDEO_PORT 41000").value() == 41000);
    TEST_CHECK(!http::parseVideoPort("VIDEO_PORT").has_value());
    TEST_CHECK(!http::parseVideoPort("VIDEO_PORT 0").has_value());
    TEST_CHECK(!http::parseVideoPort("VIDEO_PORT 65536").has_value());
    TEST_CHECK(!http::parseVideoPort("VIDEO_PORT 123abc").has_value());
    TEST_CHECK(!http::parseVideoPort("VOICE_PORT 41000").has_value());

    std::cout << "Phase 6 video control test passed\n";
}
