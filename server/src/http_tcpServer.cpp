#include <http_tcpServer.h>
#include <chat_protocol.h>

#include <liburing.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <string_view>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace http {
    namespace {
        constexpr std::size_t maximumOutgoingBytes = 64 * 1024;
    }

    struct TcpServer::Connection {
        Connection(int socket, const sockaddr_in& peer, std::uint32_t sender)
            : socket(socket), peerAddress(peer), senderId(sender) {}

        int socket;
        sockaddr_in peerAddress{};
        std::uint32_t sessionId = 1;
        std::uint32_t senderId;
        char buffer[1024]{};
        ChatFrameParser parser;
        std::string name;
        std::deque<std::string> outgoing;
        std::size_t outgoingBytes = 0;
        bool identified = false;
        bool voiceRegistered = false;
        std::size_t sendOffset = 0;
        bool sendInFlight = false;
        bool closing = false;
        std::size_t outstandingOperations = 0;
    };

    struct TcpServer::Operation {
        enum class Type { Accept, Receive, Send };

        Type type;
        Connection* connection = nullptr;
    };

    TcpServer::TcpServer(std::uint16_t port, bool bindAllInterfaces)
        : requestedPort(port), bindAllInterfaces(bindAllInterfaces) {}

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
        address.sin_addr.s_addr = htonl(
            bindAllInterfaces ? INADDR_ANY : INADDR_LOOPBACK);
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

        if (!audioTransport.start()) {
            std::cerr << "Failed to start UDP audio transport\n";
            cleanup();
            return -1;
        }
        std::cout << "Audio UDP relay listening on port " << audioTransport.port() << '\n';
        audioThread = std::thread([this] {
            while (running) {
                std::optional<media::AudioPacket> packet;
                {
                    std::lock_guard lock(audioMutex);
                    packet = audioTransport.receive(5);
                }
                if (packet) {
                    std::lock_guard lock(audioMutex);
                    audioTransport.forward(*packet);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

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
        pendingOperations.insert(operation);
        io_uring_prep_accept(submission, serverSocket, nullptr, nullptr, 0);
        io_uring_sqe_set_data(submission, operation);
        int result = io_uring_submit(ring);
        if (result <= 0) {
            pendingOperations.erase(operation);
            delete operation;
        }
        return result <= 0 ? (result < 0 ? result : -EIO) : 0;
    }

    int TcpServer::queueReceive(Connection& connection) {
        io_uring_sqe* submission = io_uring_get_sqe(ring);
        if (submission == nullptr) {
            return -ENOSPC;
        }

        auto* operation = new Operation{Operation::Type::Receive, &connection};
        pendingOperations.insert(operation);
        ++connection.outstandingOperations;
        io_uring_prep_recv(submission, connection.socket, connection.buffer,
                           sizeof(connection.buffer), 0);
        io_uring_sqe_set_data(submission, operation);
        int result = io_uring_submit(ring);
        if (result <= 0) {
            pendingOperations.erase(operation);
            --connection.outstandingOperations;
            delete operation;
        }
        return result <= 0 ? (result < 0 ? result : -EIO) : 0;
    }

    int TcpServer::queueSend(Connection& connection) {
        if (connection.outgoing.empty()) {
            connection.sendInFlight = false;
            return 0;
        }

        io_uring_sqe* submission = io_uring_get_sqe(ring);
        if (submission == nullptr) {
            return -ENOSPC;
        }

        auto* operation = new Operation{Operation::Type::Send, &connection};
        pendingOperations.insert(operation);
        ++connection.outstandingOperations;
        connection.sendInFlight = true;
        const std::string& message = connection.outgoing.front();
        io_uring_prep_send(submission, connection.socket,
                           message.data() + connection.sendOffset,
                           message.size() - connection.sendOffset, 0);
        io_uring_sqe_set_data(submission, operation);
        int result = io_uring_submit(ring);
        if (result <= 0) {
            pendingOperations.erase(operation);
            --connection.outstandingOperations;
            connection.sendInFlight = false;
            delete operation;
        }
        return result <= 0 ? (result < 0 ? result : -EIO) : 0;
    }

    void TcpServer::processCompletion(Operation* operation, int result) {
        if (operation == nullptr) {
            return;
        }

        pendingOperations.erase(operation);

        if (operation->type == Operation::Type::Accept) {
            delete operation;
            if (result >= 0) {
                sockaddr_in peer{};
                socklen_t peerLength = sizeof(peer);
                if (getpeername(result, reinterpret_cast<sockaddr*>(&peer),
                                &peerLength) < 0) {
                    close(result);
                    return;
                }
                auto connection = std::make_unique<Connection>(
                    result, peer, nextSenderId++);
                int socket = connection->socket;
                {
                    std::lock_guard lock(connectionsMutex);
                    connections.emplace(socket, std::move(connection));
                    std::cout << "Client connected; active connections: "
                              << connections.size() << '\n';
                }
                Connection* accepted = nullptr;
                {
                    std::lock_guard lock(connectionsMutex);
                    auto it = connections.find(socket);
                    if (it != connections.end()) {
                        accepted = it->second.get();
                    }
                }
                if (accepted == nullptr || queueReceive(*accepted) < 0) {
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
        if (connection != nullptr) {
            --connection->outstandingOperations;
        }
        delete operation;

        if (connection == nullptr) {
            return;
        }

        if (connection->closing || result <= 0) {
            closeConnection(connection->socket);
            return;
        }

        if (type == Operation::Type::Receive) {
            if (!handleReceived(*connection, static_cast<std::size_t>(result))) {
                closeConnection(connection->socket);
            } else if (queueReceive(*connection) < 0) {
                closeConnection(connection->socket);
            }
        } else {
            connection->sendOffset += static_cast<std::size_t>(result);
            if (connection->sendOffset < connection->outgoing.front().size()) {
                if (queueSend(*connection) < 0) {
                    closeConnection(connection->socket);
                }
            } else {
                connection->outgoingBytes -= connection->outgoing.front().size();
                connection->outgoing.pop_front();
                connection->sendOffset = 0;
                connection->sendInFlight = false;
                if (!connection->outgoing.empty()) {
                    if (queueSend(*connection) < 0) {
                        closeConnection(connection->socket);
                    }
                }
            }
        }
    }

    bool TcpServer::handleReceived(Connection& connection, std::size_t size) {
        try {
            const auto frames = connection.parser.push(
                std::string_view(connection.buffer, size));

            // Preserve the Phase 2 raw-echo contract while the Phase 3
            // newline-framed protocol is introduced.
            if (frames.empty() && size < sizeof(connection.buffer) &&
                !connection.identified &&
                std::string_view(connection.buffer, size).rfind("NAME ", 0) != 0) {
                enqueueMessage(connection,
                               std::string(connection.buffer, size));
                return true;
            }

            for (const std::string& frame : frames) {
                if (!processFrame(connection, frame)) {
                    return false;
                }
            }
        } catch (const std::length_error&) {
            return false;
        }
        return true;
    }

    bool TcpServer::processFrame(Connection& connection, const std::string& frame) {
        if (!connection.identified) {
            if (frame.rfind("NAME ", 0) != 0) {
                return false;
            }
            std::string name = frame.substr(5);
            if (!isValidDisplayName(name)) {
                return false;
            }
            connection.name = std::move(name);
            connection.identified = true;
            enqueueMessage(connection, "WELCOME " + connection.name + "\n");
            return true;
        }

        if (frame.rfind("MSG ", 0) != 0 || frame.size() == 4) {
            const auto voicePort = parseVoicePort(frame);
            if (!voicePort.has_value()) {
                return false;
            }
            sockaddr_in endpoint = connection.peerAddress;
            endpoint.sin_port = htons(*voicePort);
            std::uint16_t mediaPort = 0;
            {
                std::lock_guard lock(audioMutex);
                audioTransport.registerParticipant(connection.sessionId,
                                                   connection.senderId, endpoint);
                mediaPort = audioTransport.port();
            }
            connection.voiceRegistered = true;
            enqueueMessage(connection, "VOICE_READY " +
                                          std::to_string(connection.sessionId) + " " +
                                          std::to_string(connection.senderId) + " " +
                                          std::to_string(mediaPort) + "\n");
            return true;
        }
        broadcast(connection, "FROM " + connection.name + " " +
                              frame.substr(4) + "\n");
        return true;
    }

    void TcpServer::enqueueMessage(Connection& connection, std::string message) {
        if (connection.closing) {
            return;
        }
        if (message.size() > maximumOutgoingBytes - connection.outgoingBytes) {
            closeConnection(connection.socket);
            return;
        }
        connection.outgoingBytes += message.size();
        connection.outgoing.push_back(std::move(message));
        if (!connection.sendInFlight && queueSend(connection) < 0) {
            closeConnection(connection.socket);
        }
    }

    void TcpServer::broadcast(const Connection& sender, const std::string& message) {
        std::vector<Connection*> recipients;
        {
            std::lock_guard lock(connectionsMutex);
            for (const auto& [socket, connection] : connections) {
                if (socket != sender.socket && connection->identified &&
                    !connection->closing) {
                    recipients.push_back(connection.get());
                }
            }
        }
        for (Connection* recipient : recipients) {
            enqueueMessage(*recipient, message);
        }
    }

    void TcpServer::closeConnection(int socket) {
        std::lock_guard lock(connectionsMutex);
        auto connection = connections.find(socket);
        if (connection == connections.end()) {
            return;
        }
        Connection& client = *connection->second;
        if (client.voiceRegistered) {
            std::lock_guard audioLock(audioMutex);
            audioTransport.unregisterSender(client.sessionId, client.senderId);
            client.voiceRegistered = false;
        }
        if (!client.closing) {
            client.closing = true;
            shutdown(client.socket, SHUT_RDWR);
            close(client.socket);
        }
        if (client.outstandingOperations == 0) {
            std::cout << "Client disconnected; active connections: "
                      << connections.size() - 1 << '\n';
            connections.erase(connection);
        }
    }

    void TcpServer::closeServer() {
        running = false;
    }

    void TcpServer::cleanup() {
        running = false;
        if (audioThread.joinable()) {
            audioThread.join();
        }
        {
            std::lock_guard audioLock(audioMutex);
            audioTransport.close();
        }
        {
            std::lock_guard lock(connectionsMutex);
            for (const auto& [socket, connection] : connections) {
                if (connection->voiceRegistered) {
                    std::lock_guard audioLock(audioMutex);
                    audioTransport.unregisterSender(connection->sessionId,
                                                    connection->senderId);
                }
                if (!connection->closing) {
                    connection->closing = true;
                    shutdown(socket, SHUT_RDWR);
                    close(socket);
                }
            }
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
        for (Operation* operation : pendingOperations) {
            delete operation;
        }
        pendingOperations.clear();
        std::lock_guard lock(connectionsMutex);
        connections.clear();
    }

    std::uint16_t TcpServer::port() const {
        return boundPort.load();
    }

    std::uint16_t TcpServer::mediaPort() const {
        std::lock_guard lock(audioMutex);
        return audioTransport.port();
    }

    std::size_t TcpServer::activeConnectionCount() const {
        std::lock_guard lock(connectionsMutex);
        return connections.size();
    }
} // namespace http
