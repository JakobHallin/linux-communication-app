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

    bool sendAll(int socketFd, const std::string& message) {
        std::size_t sent = 0;
        while (sent < message.size()) {
            ssize_t result = send(socketFd, message.data() + sent,
                                  message.size() - sent, 0);
            if (result <= 0) {
                return false;
            }
            sent += static_cast<std::size_t>(result);
        }
        return true;
    }

    bool receiveLine(int socketFd, std::string& line) {
        line.clear();
        while (true) {
            pollfd descriptor{socketFd, POLLIN, 0};
            if (poll(&descriptor, 1, 1000) <= 0) {
                return false;
            }
            char character = 0;
            if (recv(socketFd, &character, 1, 0) <= 0) {
                return false;
            }
            if (character == '\n') {
                return true;
            }
            line.push_back(character);
        }
    }
}

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });
    for (int attempt = 0; attempt < 100 && server.port() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int alice = connectClient(server.port());
    int bob = connectClient(server.port());
    std::string line;
    bool passed = alice >= 0 && bob >= 0 &&
                  sendAll(alice, "NAME Alice\n") &&
                  sendAll(bob, "NAME Bob\n") &&
                  receiveLine(alice, line) && line == "WELCOME Alice" &&
                  receiveLine(bob, line) && line == "WELCOME Bob" &&
                  sendAll(alice, "MSG hello\n") &&
                  receiveLine(bob, line) && line == "FROM Alice hello";

    if (alice >= 0) close(alice);
    if (bob >= 0) close(bob);
    server.closeServer();
    serverThread.join();

    if (!passed) {
        std::cerr << "Message relay failed; received: " << line << '\n';
        return 1;
    }
    std::cout << "Phase 3 message relay test passed\n";
    return 0;
}
