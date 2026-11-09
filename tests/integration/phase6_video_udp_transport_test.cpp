#include "video_frame.h"
#include "video_udp_transport.h"

#include <arpa/inet.h>
#include "../support/check.h"
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
    sockaddr_in bindReceiver(int socketFd) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        TEST_CHECK(bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        socklen_t length = sizeof(address);
        TEST_CHECK(getsockname(socketFd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        return address;
    }

    bool hasDatagram(int socketFd) {
        pollfd descriptor{socketFd, POLLIN, 0};
        return poll(&descriptor, 1, 100) > 0;
    }
}

int main() {
    media::VideoUdpTransport transport;
    TEST_CHECK(transport.start());
    TEST_CHECK(transport.port() != 0);

    const int senderSocket = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_CHECK(senderSocket >= 0);
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(transport.port());
    inet_pton(AF_INET, "127.0.0.1", &destination.sin_addr);

    const media::VideoFrame frame{
        .sessionId = 11, .senderId = 22, .sequence = 1,
        .width = 2, .height = 2, .payload = {0xff, 0xd8, 0xff, 0xd9}};
    const auto bytes = media::serializeVideoFrame(frame);
    TEST_CHECK(sendto(senderSocket, bytes.data(), bytes.size(), 0,
                  reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) > 0);
    TEST_CHECK(!transport.receive(100).has_value());

    const int memberSocket = socket(AF_INET, SOCK_DGRAM, 0);
    const int otherSessionSocket = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_CHECK(memberSocket >= 0 && otherSessionSocket >= 0);
    const auto memberEndpoint = bindReceiver(memberSocket);
    const auto otherSessionEndpoint = bindReceiver(otherSessionSocket);
    transport.registerParticipant(11, 22, destination);
    transport.registerParticipant(11, 33, memberEndpoint);
    transport.registerParticipant(99, 44, otherSessionEndpoint);

    TEST_CHECK(sendto(senderSocket, bytes.data(), bytes.size(), 0,
                  reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) > 0);
    const auto received = transport.receive(500);
    TEST_CHECK(received.has_value());
    TEST_CHECK(received->sequence == frame.sequence);
    TEST_CHECK(transport.forward(frame) == 1);
    TEST_CHECK(hasDatagram(memberSocket));
    TEST_CHECK(!hasDatagram(otherSessionSocket));

    auto oversized = frame;
    oversized.payload.resize(media::maximumVideoDatagramBytes);
    TEST_CHECK(media::serializeVideoFrameDatagrams(oversized).size() > 1);
    TEST_CHECK(transport.forward(oversized) == 1);

    close(senderSocket);
    close(memberSocket);
    close(otherSessionSocket);
    transport.close();
    std::cout << "Phase 6 video UDP authorization test passed\n";
}
