#include <http_tcpServer.h>

#include <liburing.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace http {
    struct TcpServer::Connection {
        explicit Connection(int socket) : socket(socket) {}

        int socket;
        char buffer[1024]{};
        std::size_t sendOffset = 0;
        std::size_t sendLength = 0;
    };

    struct TcpServer::Operation {
        enum class Type { Accept, Receive, Send };

        Type type;
        Connection* connection = nullptr;
    };

    TcpServer::TcpServer(std::uint16_t port) : requestedPort(port) {}

    TcpServer::~TcpServer() {
        closeServer();
        cleanup();
    }

    int TcpServer::start() {
        ring = new io_uring{};
        int result = io_uring_queue_init(64, ring, 0);
        if (result < 0) {
            std::cerr << "Failed to initialize io_uring: " << std::strerror(-result) << '\n';
            delete ring;
            ring = nullptr;
            return -1;
        }

        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cerr << "Failed to create socket: " << std::strerror(errno) << '\n';
            cleanup();
            return -1;
        }

        int reuseAddress = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
                   &reuseAddress, sizeof(reuseAddress));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(requestedPort);

        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address),
                 sizeof(address)) < 0 || listen(serverSocket, 2) < 0) {
            std::cerr << "Failed to bind/listen: " << std::strerror(errno) << '\n';
            cleanup();
            return -1;
        }

        socklen_t addressLength = sizeof(address);
        if (getsockname(serverSocket, reinterpret_cast<sockaddr*>(&address),
                        &addressLength) < 0) {
            std::cerr << "Failed to read server port: " << std::strerror(errno) << '\n';
            cleanup();
            return -1;
        }

        boundPort = ntohs(address.sin_port);
        running = true;
        std::cout << "io_uring server listening on 127.0.0.1:" << boundPort << '\n';

        if (queueAccept() < 0) {
            cleanup();
            return -1;
        }

        while (running) {
            io_uring_cqe* completion = nullptr;
            __kernel_timespec timeout{0, 50'000'000};
            int waitResult = io_uring_wait_cqe_timeout(ring, &completion, &timeout);
            if (waitResult == -ETIME) {
                continue;
            }
            if (waitResult < 0) {
                if (running) {
                    std::cerr << "io_uring wait failed: "
                              << std::strerror(-waitResult) << '\n';
                }
                break;
            }

            auto* operation = static_cast<Operation*>(io_uring_cqe_get_data(completion));
            int completionResult = completion->res;
            io_uring_cqe_seen(ring, completion);
            processCompletion(operation, completionResult);
        }

        cleanup();
        return 0;
    }

    int TcpServer::queueAccept() {
        io_uring_sqe* submission = io_uring_get_sqe(ring);
        if (submission == nullptr) {
            return -ENOSPC;
        }

        auto* operation = new Operation{Operation::Type::Accept};
        io_uring_prep_accept(submission, serverSocket, nullptr, nullptr, 0);
        io_uring_sqe_set_data(submission, operation);
        int result = io_uring_submit(ring);
        if (result < 0) {
            delete operation;
        }
        return result < 0 ? result : 0;
    }

    int TcpServer::queueReceive(Connection& connection) {
        io_uring_sqe* submission = io_uring_get_sqe(ring);
        if (submission == nullptr) {
            return -ENOSPC;
        }

        auto* operation = new Operation{Operation::Type::Receive, &connection};
        io_uring_prep_recv(submission, connection.socket, connection.buffer,
                           sizeof(connection.buffer), 0);
        io_uring_sqe_set_data(submission, operation);
        int result = io_uring_submit(ring);
        if (result < 0) {
            delete operation;
        }
        return result < 0 ? result : 0;
    }

    int TcpServer::queueSend(Connection& connection) {
        io_uring_sqe* submission = io_uring_get_sqe(ring);
        if (submission == nullptr) {
            return -ENOSPC;
        }

        auto* operation = new Operation{Operation::Type::Send, &connection};
        io_uring_prep_send(submission, connection.socket,
                           connection.buffer + connection.sendOffset,
                           connection.sendLength - connection.sendOffset, 0);
        io_uring_sqe_set_data(submission, operation);
        int result = io_uring_submit(ring);
        if (result < 0) {
            delete operation;
        }
        return result < 0 ? result : 0;
    }

    void TcpServer::processCompletion(Operation* operation, int result) {
        if (operation == nullptr) {
            return;
        }

        if (operation->type == Operation::Type::Accept) {
            delete operation;
            if (result >= 0) {
                auto connection = std::make_unique<Connection>(result);
                int socket = connection->socket;
                {
                    std::lock_guard lock(connectionsMutex);
                    connections.emplace(socket, std::move(connection));
                    std::cout << "Client connected; active connections: "
                              << connections.size() << '\n';
                }
                if (queueReceive(*connections.at(socket)) < 0) {
                    closeConnection(socket);
                }
            }
            if (running && queueAccept() < 0) {
                running = false;
            }
            return;
        }

        Connection* connection = operation->connection;
        Operation::Type type = operation->type;
        delete operation;

        if (connection == nullptr || result <= 0) {
            if (connection != nullptr) {
                closeConnection(connection->socket);
            }
            return;
        }

        if (type == Operation::Type::Receive) {
            connection->sendOffset = 0;
            connection->sendLength = static_cast<std::size_t>(result);
            if (queueSend(*connection) < 0) {
                closeConnection(connection->socket);
            }
        } else {
            connection->sendOffset += static_cast<std::size_t>(result);
            if (connection->sendOffset < connection->sendLength) {
                if (queueSend(*connection) < 0) {
                    closeConnection(connection->socket);
                }
            } else if (queueReceive(*connection) < 0) {
                closeConnection(connection->socket);
            }
        }
    }

    void TcpServer::closeConnection(int socket) {
        std::lock_guard lock(connectionsMutex);
        auto connection = connections.find(socket);
        if (connection == connections.end()) {
            return;
        }
        shutdown(socket, SHUT_RDWR);
        close(socket);
        connections.erase(connection);
        std::cout << "Client disconnected; active connections: "
                  << connections.size() << '\n';
    }

    void TcpServer::closeServer() {
        running = false;
    }

    void TcpServer::cleanup() {
        running = false;
        {
            std::lock_guard lock(connectionsMutex);
            for (const auto& [socket, connection] : connections) {
                shutdown(socket, SHUT_RDWR);
                close(socket);
            }
            connections.clear();
        }
        if (serverSocket >= 0) {
            shutdown(serverSocket, SHUT_RDWR);
            close(serverSocket);
            serverSocket = -1;
        }
        if (ring != nullptr) {
            io_uring_queue_exit(ring);
            delete ring;
            ring = nullptr;
        }
    }

    std::uint16_t TcpServer::port() const {
        return boundPort.load();
    }

    std::size_t TcpServer::activeConnectionCount() const {
        std::lock_guard lock(connectionsMutex);
        return connections.size();
    }
} // namespace http
