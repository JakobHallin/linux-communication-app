#include "http_tcpServer.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
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
        for (int attempt = 0; attempt < 300; ++attempt) {
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
}

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });

    bool started = waitFor([&server] { return server.port() != 0; });
    int sender = started ? connectClient(server.port()) : -1;
    int slowClient = started ? connectClient(server.port()) : -1;

    int receiveBuffer = 1024;
    if (slowClient >= 0) {
        setsockopt(slowClient, SOL_SOCKET, SO_RCVBUF,
                   &receiveBuffer, sizeof(receiveBuffer));
    }

    bool connected = sender >= 0 && slowClient >= 0 &&
        waitFor([&server] { return server.activeConnectionCount() == 2; });

    const std::string handshake = "NAME Sender\n";
    const std::string slowHandshake = "NAME SlowClient\n";
    bool handshaken = connected && sendAll(sender, handshake) &&
        sendAll(slowClient, slowHandshake);

    // The slow client intentionally never reads its welcome or relayed data.
    std::string flood;
    for (int index = 0; index < 1024; ++index) {
        flood += "MSG " + std::string(900, 'x') + " " +
                 std::to_string(index) + "\n";
    }
    bool flooded = handshaken && sendAll(sender, flood);
    bool slowClientRemoved = flooded &&
        waitFor([&server] { return server.activeConnectionCount() == 1; });

    if (sender >= 0) close(sender);
    if (slowClient >= 0) close(slowClient);
    server.closeServer();
    serverThread.join();

    if (!slowClientRemoved) {
        std::cerr << "Slow-client limit did not disconnect the stalled client\n";
        return 1;
    }

    std::cout << "Phase 3 slow-client test passed\n";
    return 0;
}
