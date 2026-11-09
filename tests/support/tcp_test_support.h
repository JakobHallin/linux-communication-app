#pragma once

#include "http_tcpServer.h"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <iostream>
#include <poll.h>
#include <string>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace test {
    using Clock = std::chrono::steady_clock;
    constexpr auto timeout = std::chrono::seconds(2);

    inline void require(bool condition, const std::string& message) {
        if (!condition) throw std::runtime_error(message);
    }

    template <typename Condition>
    bool waitFor(const Condition& condition,
                 std::chrono::milliseconds limit = timeout) {
        const auto deadline = Clock::now() + limit;
        do {
            if (condition()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (Clock::now() < deadline);
        return condition();
    }

    template <typename Scenario>
    int run(const std::string& name, const Scenario& scenario) {
        try {
            scenario();
            std::cout << name << " passed\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << name << " failed: " << error.what() << '\n';
            return 1;
        }
    }

    class RunningServer {
    public:
        RunningServer() : thread_([this] {
            server.start();
            finished_ = true;
        }) {}
        ~RunningServer() {
            // Startup may still be in progress when a readiness check fails.
            // Repeat the stop request so a late start cannot overwrite it.
            do {
                server.closeServer();
                if (!finished_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } while (!finished_);
            thread_.join();
        }

        http::TcpServer server{0};

    private:
        std::atomic<bool> finished_{false};
        std::thread thread_;
    };

    class ClientSocket {
    public:
        explicit ClientSocket(int fd) : fd_(fd) {}
        ~ClientSocket() { if (fd_ >= 0) close(fd_); }
        ClientSocket(const ClientSocket&) = delete;
        ClientSocket& operator=(const ClientSocket&) = delete;
        int get() const { return fd_; }
        void reset() {
            if (fd_ >= 0) close(fd_);
            fd_ = -1;
        }

    private:
        int fd_;
    };

    inline int connectClient(std::uint16_t port) {
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

    inline bool sendAll(int socketFd, const std::string& message) {
        std::size_t sent = 0;
        while (sent < message.size()) {
            ssize_t result = send(socketFd, message.data() + sent,
                                  message.size() - sent, MSG_NOSIGNAL);
            if (result < 0 && errno == EINTR) continue;
            if (result <= 0) {
                return false;
            }
            sent += static_cast<std::size_t>(result);
        }
        return true;
    }

    inline bool receiveLine(int socketFd, std::string& line) {
        line.clear();
        const auto deadline = Clock::now() + timeout;
        while (Clock::now() < deadline) {
            const auto remaining = std::chrono::duration_cast<
                std::chrono::milliseconds>(deadline - Clock::now()).count();
            if (remaining <= 0) return false;
            pollfd descriptor{socketFd, POLLIN, 0};
            const int ready = poll(&descriptor, 1, static_cast<int>(remaining));
            if (ready < 0 && errno == EINTR) continue;
            if (ready <= 0) {
                return false;
            }
            char character = 0;
            const ssize_t received = recv(socketFd, &character, 1, 0);
            if (received < 0 && errno == EINTR) continue;
            if (received <= 0) {
                return false;
            }
            if (character == '\n') {
                return true;
            }
            line.push_back(character);
        }
        return false;
    }

    inline void expectLine(int socketFd, const std::string& expected,
                    const std::string& step) {
        std::string actual;
        const bool received = receiveLine(socketFd, actual);
        require(received,
                step + ": could not read a complete line; received '" + actual + "'");
        require(actual == expected,
                step + ": expected '" + expected + "', received '" + actual + "'");
    }
}
