#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
    bool receiveLine(int socketFd, std::string& line) {
        line.clear();
        while (true) {
            pollfd descriptor{socketFd, POLLIN, 0};
            if (poll(&descriptor, 1, 1000) <= 0) return false;
            char character = 0;
            if (recv(socketFd, &character, 1, 0) <= 0) return false;
            if (character == '\n') return true;
            line.push_back(character);
        }
    }
}

int main() {
    const char* host = std::getenv("COMMUNICATION_SERVER_HOST");
    const char* portText = std::getenv("COMMUNICATION_SERVER_PORT");
    if (host == nullptr || portText == nullptr) {
        std::cerr << "COMMUNICATION_SERVER_HOST and COMMUNICATION_SERVER_PORT are required\n";
        return 1;
    }

    int port = std::atoi(portText);
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0 || port <= 0 || port > 65535) {
        std::cerr << "Could not create a valid client socket\n";
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1 ||
        connect(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Could not connect to container server: " << std::strerror(errno) << '\n';
        close(socketFd);
        return 1;
    }

    constexpr char handshake[] = "NAME ContainerClient\n";
    bool sent = send(socketFd, handshake, sizeof(handshake) - 1, 0) > 0;
    std::string response;
    bool passed = sent && receiveLine(socketFd, response) &&
                  response == "WELCOME ContainerClient";

    close(socketFd);
    if (!passed) {
        std::cerr << "Container server handshake failed; received: " << response << '\n';
        return 1;
    }

    std::cout << "Phase 4 container connection test passed\n";
    return 0;
}
