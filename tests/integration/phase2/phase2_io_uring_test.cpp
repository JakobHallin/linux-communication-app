#include "http_tcpServer.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
    int connectClient(std::uint16_t port) {
        int socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) {
            return -1;
        }

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
            if (condition()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
}

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });

    if (!waitFor([&server] { return server.port() != 0; })) {
        std::cerr << "io_uring server did not start\n";
        server.closeServer();
        serverThread.join();
        return 1;
    }

    int clientSocket = connectClient(server.port());
    if (clientSocket < 0) {
        std::cerr << "Could not connect to io_uring server\n";
        server.closeServer();
        serverThread.join();
        return 1;
    }

    constexpr char message[] = "phase2 round trip";
    if (send(clientSocket, message, sizeof(message), 0) != sizeof(message)) {
        std::cerr << "Could not send test message\n";
        close(clientSocket);
        server.closeServer();
        serverThread.join();
        return 1;
    }

    char response[sizeof(message)]{};
    ssize_t received = recv(clientSocket, response, sizeof(response), 0);
    bool passed = received == sizeof(message) &&
                  std::memcmp(response, message, sizeof(message)) == 0;

    close(clientSocket);
    server.closeServer();
    serverThread.join();

    if (!passed) {
        std::cerr << "io_uring round trip failed\n";
        return 1;
    }

    std::cout << "Phase 2 io_uring round-trip test passed\n";
    return 0;
}
