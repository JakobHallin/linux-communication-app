#ifndef INCLUDE_HTTP_TCP_SERVER_H
#define INCLUDE_HTTP_TCP_SERVER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace http {
    class TcpServer {
    public:
        explicit TcpServer(std::uint16_t port = 8080);
        ~TcpServer();
        TcpServer(const TcpServer&) = delete;
        TcpServer& operator=(const TcpServer&) = delete;

        int start();
        void closeServer();
        std::uint16_t port() const;
        std::size_t activeConnectionCount() const;

    private:
        void handleClient(int clientSocket);

        int serverSocket = -1;
        std::uint16_t requestedPort;
        std::atomic<std::uint16_t> boundPort{0};
        std::atomic<bool> running{false};
        mutable std::mutex clientsMutex;
        std::unordered_set<int> clients;
        std::vector<std::thread> clientThreads;
    };
} // namespace http

#endif // INCLUDE_HTTP_TCP_SERVER_H
