#include "chat_protocol.h"
#include "../support/check.h"

#include <iostream>
#include <string>

int main() {
    // TCP may split even the command prefix at any byte boundary.
    const std::string handshake = "NAME Alice\n";
    for (std::size_t split = 1; split < handshake.size(); ++split) {
        http::ChatFrameParser handshakeParser;
        TEST_CHECK(handshakeParser.push(handshake.substr(0, split)).empty());
        const auto completed = handshakeParser.push(handshake.substr(split));
        TEST_CHECK(completed.size() == 1);
        TEST_CHECK(completed.front() == "NAME Alice");
    }

    http::ChatFrameParser parser;

    if (!parser.push("MSG hello").empty()) {
        std::cerr << "A partial frame must not be emitted\n";
        return 1;
    }

    const auto frames = parser.push("\nMSG second\n");
    if (frames.size() != 2 || frames[0] != "MSG hello" ||
        frames[1] != "MSG second") {
        std::cerr << "Partial and combined frames were parsed incorrectly\n";
        return 1;
    }

    std::cout << "Phase 3 framing test passed\n";
    return 0;
}
