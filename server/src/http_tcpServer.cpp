#include <http_tcpServer.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace http {
    TcpServer::TcpServer(std::uint16_t port) : requestedPort(port) {}

    TcpServer::~TcpServer() {
        closeServer();
    }

    int TcpServer::start() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cerr << "Failed to create socket: " << std::strerror(errno) << '\n';
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
                 sizeof(address)) < 0) {
            std::cerr << "Failed to bind socket: " << std::strerror(errno) << '\n';
            closeServer();
            return -1;
        }

        socklen_t addressLength = sizeof(address);
        if (getsockname(serverSocket, reinterpret_cast<sockaddr*>(&address),
                        &addressLength) < 0 || listen(serverSocket, 2) < 0) {
            std::cerr << "Failed to listen on socket: " << std::strerror(errno) << '\n';
            closeServer();
            return -1;
        }

        boundPort = ntohs(address.sin_port);
        running = true;
        std::cout << "TCP server listening on 127.0.0.1:" << boundPort << '\n';

        while (running) {
            int clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket < 0) {
                if (running) {
                    std::cerr << "Failed to accept client: " << std::strerror(errno) << '\n';
                }
                continue;
            }

            {
                std::lock_guard lock(clientsMutex);
                clients.insert(clientSocket);
                std::cout << "Client connected; active connections: "
                          << clients.size() << '\n';
            }
            clientThreads.emplace_back(&TcpServer::handleClient, this, clientSocket);
        }

        return 0;
    }

    void TcpServer::handleClient(int clientSocket) {
        char buffer[1024];
        while (running) {
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) {
                break;
            }
        }

        close(clientSocket);
        std::lock_guard lock(clientsMutex);
        clients.erase(clientSocket);
        std::cout << "Client disconnected; active connections: "
                  << clients.size() << '\n';
    }

    void TcpServer::closeServer() {
        bool wasRunning = running.exchange(false);
        if (serverSocket >= 0) {
            shutdown(serverSocket, SHUT_RDWR);
            close(serverSocket);
            serverSocket = -1;
        }

        if (wasRunning) {
            std::lock_guard lock(clientsMutex);
            for (int clientSocket : clients) {
                shutdown(clientSocket, SHUT_RDWR);
            }
        }

        for (std::thread& clientThread : clientThreads) {
            if (clientThread.joinable()) {
                clientThread.join();
            }
        }
        clientThreads.clear();
    }

    std::uint16_t TcpServer::port() const {
        return boundPort.load();
    }

    std::size_t TcpServer::activeConnectionCount() const {
        std::lock_guard lock(clientsMutex);
        return clients.size();
    }
} // namespace http
