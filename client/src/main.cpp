#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Failed to create client socket: " << std::strerror(errno) << '\n';
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress),
                sizeof(serverAddress)) < 0) {
        std::cerr << "Failed to connect to 127.0.0.1:8080: "
                  << std::strerror(errno) << '\n';
        close(clientSocket);
        return 1;
    }

    std::cout << "Connected to server. Press Enter to disconnect.\n";
    std::string line;
    std::getline(std::cin, line);

    shutdown(clientSocket, SHUT_RDWR);
    close(clientSocket);
    std::cout << "Disconnected.\n";
    return 0;
}
