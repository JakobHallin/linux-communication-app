#ifndef INCLUDE_HTTP_TCP_SERVER_H
#define INCLUDE_HTTP_TCP_SERVER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>

struct io_uring;

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
        struct Connection;
        struct Operation;

        int queueAccept();
        int queueReceive(Connection& connection);
        int queueSend(Connection& connection);
        void closeConnection(int socket);
        void processCompletion(Operation* operation, int result);
        void cleanup();

        int serverSocket = -1;
        std::uint16_t requestedPort;
        std::atomic<std::uint16_t> boundPort{0};
        std::atomic<bool> running{false};
        io_uring* ring = nullptr;
        std::unordered_map<int, std::unique_ptr<Connection>> connections;
        std::unordered_set<Operation*> pendingOperations;
        mutable std::mutex connectionsMutex;
    };
} // namespace http

#endif // INCLUDE_HTTP_TCP_SERVER_H
