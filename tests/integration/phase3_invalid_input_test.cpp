#include "http_tcpServer.h"

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

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

    bool serverClosed(int socketFd) {
        pollfd descriptor{socketFd, POLLIN | POLLHUP, 0};
        if (poll(&descriptor, 1, 1000) <= 0) return false;
        char byte = 0;
        return recv(socketFd, &byte, 1, 0) == 0;
    }
}

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });
    if (!waitFor([&server] { return server.port() != 0; })) return 1;

    int invalid = connectClient(server.port());
    std::string malformed = "BAD frame\n";
    bool invalidRejected = invalid >= 0 &&
        send(invalid, malformed.data(), malformed.size(), 0) > 0 &&
        serverClosed(invalid);
    if (invalid >= 0) close(invalid);

    int oversized = connectClient(server.port());
    std::string oversizedFrame(2048, 'x');
    bool oversizedRejected = oversized >= 0 &&
        send(oversized, oversizedFrame.data(), oversizedFrame.size(), 0) > 0 &&
        serverClosed(oversized);
    if (oversized >= 0) close(oversized);

    int valid = connectClient(server.port());
    const std::string handshake = "NAME Alice\n";
    bool serverSurvived = valid >= 0 &&
        send(valid, handshake.data(), handshake.size(), 0) > 0 &&
        waitFor([&server] { return server.activeConnectionCount() == 1; });
    if (valid >= 0) close(valid);

    server.closeServer();
    serverThread.join();

    if (!invalidRejected || !oversizedRejected || !serverSurvived) {
        std::cerr << "Invalid-input handling failed\n";
        return 1;
    }
    std::cout << "Phase 3 invalid-input test passed\n";
    return 0;
}
