#include "chat_protocol.h"

#include <iostream>

int main() {
    if (!http::isValidDisplayName("Alice_01")) {
        std::cerr << "A valid display name was rejected\n";
        return 1;
    }

    if (http::isValidDisplayName("") ||
        http::isValidDisplayName("name with spaces") ||
        http::isValidDisplayName("name!")) {
        std::cerr << "An invalid display name was accepted\n";
        return 1;
    }

    std::cout << "Phase 3 display-name test passed\n";
    return 0;
}
