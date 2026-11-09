#include "http_tcpServer.h"
#include "screen_frame_encoder.h"
#include "text_client.h"
#include "video_frame.h"

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    sockaddr_in bindUdpSocket(int socketFd) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        assert(bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        socklen_t length = sizeof(address);
        assert(getsockname(socketFd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        return address;
    }

    bool receivesDatagram(int socketFd, int timeoutMilliseconds) {
        pollfd descriptor{socketFd, POLLIN, 0};
        return poll(&descriptor, 1, timeoutMilliseconds) > 0;
    }
}

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });
    for (int attempt = 0; attempt < 100 && server.port() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(server.port() != 0);

    client::TextClient sender("127.0.0.1", server.port());
    client::TextClient viewer("127.0.0.1", server.port());
    assert(sender.connectToServer() && viewer.connectToServer());
    assert(sender.sendName("ScreenSender") && viewer.sendName("ScreenViewer"));
    assert(sender.receiveLine(1000) == "WELCOME ScreenSender");
    assert(viewer.receiveLine(1000) == "WELCOME ScreenViewer");

    const int senderSocket = socket(AF_INET, SOCK_DGRAM, 0);
    const int viewerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    assert(senderSocket >= 0 && viewerSocket >= 0);
    const auto senderEndpoint = bindUdpSocket(senderSocket);
    const auto viewerEndpoint = bindUdpSocket(viewerSocket);
    const auto senderReady = sender.registerVideo(ntohs(senderEndpoint.sin_port), 1000);
    const auto viewerReady = viewer.registerVideo(ntohs(viewerEndpoint.sin_port), 1000);
    assert(senderReady.has_value() && viewerReady.has_value());

    const media::BgraFrame source{
        .width = 2, .height = 2, .stride = 8,
        .pixels = {0, 0, 255, 255, 0, 255, 0, 255,
                   255, 0, 0, 255, 255, 255, 255, 255}
    };
    const auto frame = media::makeMjpegVideoFrameFromBgra(
        senderReady->sessionId, senderReady->senderId, 1, source);
    assert(frame.has_value());
    const auto bytes = media::serializeVideoFrame(*frame);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddress.sin_port = htons(server.videoMediaPort());
    assert(sendto(senderSocket, bytes.data(), bytes.size(), 0,
                  reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) > 0);
    assert(receivesDatagram(viewerSocket, 1000));

    sender.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    while (receivesDatagram(viewerSocket, 0)) {
        std::vector<std::uint8_t> discarded(media::maximumVideoDatagramBytes + 26);
        recvfrom(viewerSocket, discarded.data(), discarded.size(), 0, nullptr, nullptr);
    }
    assert(sendto(senderSocket, bytes.data(), bytes.size(), 0,
                  reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) > 0);
    assert(!receivesDatagram(viewerSocket, 250));

    viewer.close();
    close(senderSocket);
    close(viewerSocket);
    server.closeServer();
    serverThread.join();
    std::cout << "Phase 7 screen-share integration test passed\n";
}
