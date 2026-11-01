#include "http_tcpServer.h"

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    int connectClient(std::uint16_t port) {
        int socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) return -1;

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (connect(socketFd, reinterpret_cast<sockaddr*>(&address),
                    sizeof(address)) < 0) {
            close(socketFd);
            return -1;
        }
        return socketFd;
    }

    bool waitFor(const auto& condition) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            if (condition()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
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
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });

    bool started = waitFor([&server] { return server.port() != 0; });
    std::vector<int> clients;
    if (started) {
        for (int index = 0; index < 3; ++index) {
            clients.push_back(connectClient(server.port()));
        }
    }

    bool connected = clients.size() == 3 && clients[0] >= 0 &&
        clients[1] >= 0 && clients[2] >= 0 &&
        waitFor([&server] { return server.activeConnectionCount() == 3; });

    bool handshaken = connected;
    std::string line;
    for (int index = 0; handshaken && index < 3; ++index) {
        handshaken = sendAll(clients[index],
                             "NAME Client" + std::to_string(index + 1) + "\n");
    }
    for (int index = 0; handshaken && index < 3; ++index) {
        handshaken = receiveLine(clients[index], line) &&
            line == "WELCOME Client" + std::to_string(index + 1);
    }

    bool firstBroadcast = handshaken &&
        sendAll(clients[0], "MSG from-one\n") &&
        receiveLine(clients[1], line) && line == "FROM Client1 from-one" &&
        receiveLine(clients[2], line) && line == "FROM Client1 from-one";

    bool secondBroadcast = firstBroadcast &&
        sendAll(clients[1], "MSG from-two\n") &&
        receiveLine(clients[0], line) && line == "FROM Client2 from-two" &&
        receiveLine(clients[2], line) && line == "FROM Client2 from-two";

    for (int socketFd : clients) {
        if (socketFd >= 0) close(socketFd);
    }
    server.closeServer();
    serverThread.join();

    if (!secondBroadcast) {
        std::cerr << "Three-client relay test failed\n";
        return 1;
    }

    std::cout << "Phase 4 multi-client test passed\n";
    return 0;
}
