#include <http_tcpServer.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>

namespace http {
    TcpServer::TcpServer() {
        // Constructor implementation
    }

    TcpServer::~TcpServer() {
        closeServer();
    }

    int TcpServer::start() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return -1;
        }

        int reuseAddress = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
                   &reuseAddress, sizeof(reuseAddress));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(8080);

        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address),
                 sizeof(address)) < 0) {
            std::cerr << "Failed to bind socket to port 8080" << std::endl;
            closeServer();
            return -1;
        }

        if (listen(serverSocket,  SOMAXCONN) < 0) {
            std::cerr << "Failed to listen on socket" << std::endl;
            closeServer();
            return -1;
        }

        std::cout << "TCP server listening on port 8080" << std::endl;

        while (true) {
            int clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket < 0) {
                std::cerr << "Failed to accept client connection" << std::endl;
                continue;
            }

            const char response[] =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 22\r\n"
                "Connection: close\r\n\r\n"
                "Hello from TCP server\n";
            send(clientSocket, response, std::strlen(response), 0);
            close(clientSocket);
        }

        return 0;
    }
    void TcpServer::closeServer() {
        if (serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }
    }
} // namespace http
