#include "text_client.h"
#include "video_frame.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = argc > 2 ? static_cast<std::uint16_t>(std::stoul(argv[2])) : 8080;
    const std::string name = argc > 3 ? argv[3] : "Viewer";
    const int seconds = argc > 4 ? std::atoi(argv[4]) : 0;
    const bool streamMjpeg = argc > 5 && std::string(argv[5]) == "--stdout-mjpeg";

    client::TextClient textClient(host, port);
    if (!textClient.connectToServer() || !textClient.sendName(name)) {
        std::cerr << "Could not connect viewer to server\n";
        return 1;
    }
    const auto welcome = textClient.receiveLine(1000);
    if (!welcome.has_value()) {
        std::cerr << "Viewer did not receive a welcome\n";
        return 1;
    }

    const int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd < 0) {
        std::cerr << "Could not create viewer UDP socket\n";
        return 1;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(0);
    if (bind(socketFd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
        close(socketFd);
        std::cerr << "Could not bind viewer UDP socket\n";
        return 1;
    }
    socklen_t localLength = sizeof(local);
    getsockname(socketFd, reinterpret_cast<sockaddr*>(&local), &localLength);

    const auto ready = textClient.registerVideo(ntohs(local.sin_port), 1000);
    if (!ready.has_value()) {
        close(socketFd);
        std::cerr << "Server did not acknowledge viewer video registration\n";
        return 1;
    }

    if (!streamMjpeg) {
        std::cout << *welcome << '\n'
                  << "Video viewer ready on UDP port " << ntohs(local.sin_port)
                  << "\n";
    }

    const auto deadline = seconds > 0
        ? std::chrono::steady_clock::now() + std::chrono::seconds(seconds)
        : std::chrono::steady_clock::time_point::max();
    std::vector<std::uint8_t> buffer(media::maximumVideoFrameBytes + 26);
    int receivedFrames = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        pollfd descriptor{socketFd, POLLIN, 0};
        if (poll(&descriptor, 1, 250) <= 0) continue;
        const auto received = recvfrom(socketFd, buffer.data(), buffer.size(), 0,
                                       nullptr, nullptr);
        if (received <= 0) continue;
        buffer.resize(static_cast<std::size_t>(received));
        const auto frame = media::deserializeVideoFrame(buffer);
        buffer.resize(media::maximumVideoFrameBytes + 26);
        if (!frame.has_value()) continue;

        if (streamMjpeg) {
            std::cout.write(reinterpret_cast<const char*>(frame->payload.data()),
                            static_cast<std::streamsize>(frame->payload.size()));
            std::cout.flush();
        } else {
            std::cout << "Received video frame " << ++receivedFrames << ": "
                      << frame->width << 'x' << frame->height << ", "
                      << frame->payload.size() << " bytes\n";
        }
    }

    close(socketFd);
    textClient.close();
    if (!streamMjpeg) {
        std::cout << "Video viewer stopped after " << receivedFrames << " frame(s)\n";
    }
    return 0;
}
