#include "text_client.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

namespace client {
    TextClient::TextClient(std::string serverHost, std::uint16_t serverPort)
        : host(std::move(serverHost)), port(serverPort) {}

    TextClient::~TextClient() { close(); }

    bool TextClient::connectToServer() {
        if (socketFd >= 0) return true;
        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) return false;

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1 ||
            connect(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            close();
            return false;
        }
        return true;
    }

    bool TextClient::sendName(const std::string& name) {
        return sendLine("NAME " + name);
    }

    bool TextClient::sendMessage(const std::string& message) {
        return sendLine("MSG " + message);
    }

    bool TextClient::sendLine(const std::string& line) {
        if (socketFd < 0) return false;
        const std::string framed = line + "\n";
        std::size_t sent = 0;
        while (sent < framed.size()) {
            const auto result = send(socketFd, framed.data() + sent,
                                     framed.size() - sent, MSG_NOSIGNAL);
            if (result <= 0) return false;
            sent += static_cast<std::size_t>(result);
        }
        return true;
    }

    std::optional<std::string> TextClient::receiveLine(int timeoutMilliseconds) {
        if (socketFd < 0 || timeoutMilliseconds < 0) return std::nullopt;
        std::string line;
        while (true) {
            pollfd descriptor{socketFd, POLLIN, 0};
            if (poll(&descriptor, 1, timeoutMilliseconds) <= 0) return std::nullopt;
            char character = 0;
            if (recv(socketFd, &character, 1, 0) <= 0) return std::nullopt;
            if (character == '\n') return line;
            if (character != '\r') line.push_back(character);
        }
    }

    void TextClient::close() {
        if (socketFd >= 0) {
            shutdown(socketFd, SHUT_RDWR);
            ::close(socketFd);
            socketFd = -1;
        }
    }
}
