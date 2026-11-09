#include "http_tcpServer.h"
#include "text_client.h"
#include "video_frame.h"

#include <arpa/inet.h>
#include "../support/check.h"
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
        address.sin_port = 0;
        TEST_CHECK(bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        socklen_t length = sizeof(address);
        TEST_CHECK(getsockname(socketFd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        return address;
    }
}

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });
    for (int attempt = 0; attempt < 100 && server.port() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_CHECK(server.port() != 0);

    client::TextClient sender("127.0.0.1", server.port());
    client::TextClient receiver("127.0.0.1", server.port());
    TEST_CHECK(sender.connectToServer());
    TEST_CHECK(receiver.connectToServer());
    TEST_CHECK(sender.sendName("Alice"));
    TEST_CHECK(receiver.sendName("Bob"));
    TEST_CHECK(sender.receiveLine(1000) == "WELCOME Alice");
    TEST_CHECK(receiver.receiveLine(1000) == "WELCOME Bob");

    const int senderSocket = socket(AF_INET, SOCK_DGRAM, 0);
    const int receiverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_CHECK(senderSocket >= 0 && receiverSocket >= 0);
    const auto senderEndpoint = bindUdpSocket(senderSocket);
    const auto receiverEndpoint = bindUdpSocket(receiverSocket);
    const auto senderReady = sender.registerVideo(ntohs(senderEndpoint.sin_port), 1000);
    const auto receiverReady = receiver.registerVideo(ntohs(receiverEndpoint.sin_port), 1000);
    TEST_CHECK(senderReady.has_value() && receiverReady.has_value());
    TEST_CHECK(senderReady->sessionId == receiverReady->sessionId);
    TEST_CHECK(senderReady->mediaPort == server.videoMediaPort());
    TEST_CHECK(receiverReady->mediaPort == server.videoMediaPort());

    const media::VideoFrame frame{
        .sessionId = senderReady->sessionId,
        .senderId = senderReady->senderId,
        .sequence = 7,
        .width = 2,
        .height = 2,
        .payload = {0xff, 0xd8, 0x01, 0x02, 0xff, 0xd9}
    };
    const auto encoded = media::serializeVideoFrame(frame);
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddress.sin_port = htons(server.videoMediaPort());
    TEST_CHECK(sendto(senderSocket, encoded.data(), encoded.size(), 0,
                  reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) > 0);

    pollfd descriptor{receiverSocket, POLLIN, 0};
    TEST_CHECK(poll(&descriptor, 1, 1000) > 0);
    std::vector<std::uint8_t> received(media::maximumVideoFrameBytes + 26);
    const auto receivedBytes = recvfrom(receiverSocket, received.data(), received.size(), 0,
                                        nullptr, nullptr);
    TEST_CHECK(receivedBytes > 0);
    received.resize(static_cast<std::size_t>(receivedBytes));
    const auto forwarded = media::deserializeVideoFrame(received);
    TEST_CHECK(forwarded.has_value());
    TEST_CHECK(forwarded->sessionId == frame.sessionId);
    TEST_CHECK(forwarded->senderId == frame.senderId);
    TEST_CHECK(forwarded->sequence == frame.sequence);
    TEST_CHECK(forwarded->payload == frame.payload);

    sender.close();
    receiver.close();
    close(senderSocket);
    close(receiverSocket);
    server.closeServer();
    serverThread.join();
    std::cout << "Phase 6 video registration integration test passed\n";
}
