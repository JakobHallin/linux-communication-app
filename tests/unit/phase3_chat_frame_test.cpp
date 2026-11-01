#include "chat_protocol.h"

#include <iostream>
#include <string>

int main() {
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
