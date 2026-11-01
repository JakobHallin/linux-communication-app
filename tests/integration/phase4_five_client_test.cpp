#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {
    int connectClient(const char* host, int port) {
        int socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) return -1;

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<std::uint16_t>(port));
        if (inet_pton(AF_INET, host, &address.sin_addr) != 1 ||
            connect(socketFd, reinterpret_cast<sockaddr*>(&address),
                    sizeof(address)) < 0) {
            close(socketFd);
            return -1;
        }
        return socketFd;
    }

    bool sendAll(int socketFd, const std::string& message) {
        std::size_t sent = 0;
        while (sent < message.size()) {
            ssize_t result = send(socketFd, message.data() + sent,
                                  message.size() - sent, 0);
            if (result <= 0) return false;
            sent += static_cast<std::size_t>(result);
        }
        return true;
    }

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
    std::vector<int> clients;
    for (int index = 0; index < 5; ++index) {
        clients.push_back(connectClient(host, port));
    }

    bool connected = port > 0 && clients.size() == 5;
    for (int socketFd : clients) {
        connected = connected && socketFd >= 0;
    }

    std::string line;
    bool handshaken = connected;
    for (int index = 0; handshaken && index < 5; ++index) {
        handshaken = sendAll(clients[index],
                             "NAME Client" + std::to_string(index + 1) + "\n");
    }
    for (int index = 0; handshaken && index < 5; ++index) {
        handshaken = receiveLine(clients[index], line) &&
            line == "WELCOME Client" + std::to_string(index + 1);
    }

    bool broadcasted = handshaken && sendAll(clients[0], "MSG five-client-test\n");
    for (int index = 1; broadcasted && index < 5; ++index) {
        broadcasted = receiveLine(clients[index], line) &&
            line == "FROM Client1 five-client-test";
    }

    for (int socketFd : clients) {
        if (socketFd >= 0) close(socketFd);
    }

    if (!broadcasted) {
        std::cerr << "Five-client container relay test failed\n";
        return 1;
    }

    std::cout << "Phase 4 five-client test passed\n";
    return 0;
}
