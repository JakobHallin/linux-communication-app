#include "chat_protocol.h"

#include <cassert>
#include <iostream>

int main() {
    assert(http::parseVideoPort("VIDEO_PORT 41000").value() == 41000);
    assert(!http::parseVideoPort("VIDEO_PORT").has_value());
    assert(!http::parseVideoPort("VIDEO_PORT 0").has_value());
    assert(!http::parseVideoPort("VIDEO_PORT 65536").has_value());
    assert(!http::parseVideoPort("VIDEO_PORT 123abc").has_value());
    assert(!http::parseVideoPort("VOICE_PORT 41000").has_value());

    std::cout << "Phase 6 video control test passed\n";
}
