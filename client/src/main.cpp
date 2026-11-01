#include "text_client.h"

#include <atomic>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = argc > 2 ? static_cast<std::uint16_t>(std::stoul(argv[2])) : 8080;
    const std::string name = argc > 3 ? argv[3] : "Client";

    client::TextClient textClient(host, port);
    if (!textClient.connectToServer()) {
        std::cerr << "Failed to connect to " << host << ':' << port << '\n';
        return 1;
    }
    if (!textClient.sendName(name)) {
        std::cerr << "Failed to send display name\n";
        textClient.close();
        return 1;
    }
    const auto welcome = textClient.receiveLine(1000);
    if (!welcome.has_value()) {
        std::cerr << "Server did not acknowledge the display name\n";
        textClient.close();
        return 1;
    }

    std::cout << *welcome << '\n'
              << "Connected. Type messages; use /quit to disconnect.\n";
    std::atomic_bool running{true};
    std::thread receiver([&] {
        while (running) {
            const auto line = textClient.receiveLine(250);
            if (line.has_value()) {
                std::cout << *line << '\n';
            }
        }
    });

    std::string line;
    while (std::getline(std::cin, line) && line != "/quit") {
        if (!line.empty() && !textClient.sendMessage(line)) {
            std::cerr << "Failed to send message\n";
            break;
        }
    }
    running = false;
    textClient.close();
    receiver.join();
    std::cout << "Disconnected.\n";
    return 0;
}
