#ifndef INCLUDE_HTTP_TCP_SERVER_H
#define INCLUDE_HTTP_TCP_SERVER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <thread>

#include "audio_udp_transport.h"

struct io_uring;

namespace http {
    class TcpServer {
    public:
        explicit TcpServer(std::uint16_t port = 8080,
                           bool bindAllInterfaces = false);
        ~TcpServer();
        TcpServer(const TcpServer&) = delete;
        TcpServer& operator=(const TcpServer&) = delete;

        int start();
        void closeServer();
        std::uint16_t port() const;
        std::uint16_t mediaPort() const;
        std::size_t activeConnectionCount() const;

    private:
        struct Connection;
        struct Operation;

        int queueAccept();
        int queueReceive(Connection& connection);
        int queueSend(Connection& connection);
        void closeConnection(int socket);
        bool handleReceived(Connection& connection, std::size_t size);
        bool processFrame(Connection& connection, const std::string& frame);
        void enqueueMessage(Connection& connection, std::string message);
        void broadcast(const Connection& sender, const std::string& message);
        void processCompletion(Operation* operation, int result);
        void cleanup();

        int serverSocket = -1;
        std::uint16_t requestedPort;
        bool bindAllInterfaces;
        std::atomic<std::uint16_t> boundPort{0};
        std::atomic<std::uint32_t> nextSenderId{1};
        std::atomic<bool> running{false};
        io_uring* ring = nullptr;
        std::unordered_map<int, std::unique_ptr<Connection>> connections;
        std::unordered_set<Operation*> pendingOperations;
        mutable std::mutex connectionsMutex;
        media::AudioUdpTransport audioTransport;
        std::thread audioThread;
        mutable std::mutex audioMutex;
    };
} // namespace http

#endif // INCLUDE_HTTP_TCP_SERVER_H
