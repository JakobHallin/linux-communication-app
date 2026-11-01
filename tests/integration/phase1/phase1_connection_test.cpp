#include "http_tcpServer.h"

#include <arpa/inet.h>
#include <chrono>
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
        std::cerr << "Server did not start\n";
        server.closeServer();
        serverThread.join();
        return 1;
    }

    int firstClient = connectClient(server.port());
    if (firstClient < 0 || !waitFor([&server] {
            return server.activeConnectionCount() == 1;
        })) {
        std::cerr << "First client did not connect\n";
        if (firstClient >= 0) close(firstClient);
        server.closeServer();
        serverThread.join();
        return 1;
    }

    int secondClient = connectClient(server.port());
    if (secondClient < 0 || !waitFor([&server] {
            return server.activeConnectionCount() == 2;
        })) {
        std::cerr << "Second client did not connect\n";
        close(firstClient);
        if (secondClient >= 0) close(secondClient);
        server.closeServer();
        serverThread.join();
        return 1;
    }

    close(firstClient);
    if (!waitFor([&server] { return server.activeConnectionCount() == 1; })) {
        std::cerr << "First disconnect was not reported\n";
        close(secondClient);
        server.closeServer();
        serverThread.join();
        return 1;
    }

    close(secondClient);
    if (!waitFor([&server] { return server.activeConnectionCount() == 0; })) {
        std::cerr << "Second disconnect was not reported\n";
        server.closeServer();
        serverThread.join();
        return 1;
    }

    server.closeServer();
    serverThread.join();
    std::cout << "Phase 1 connection test passed\n";
    return 0;
}
