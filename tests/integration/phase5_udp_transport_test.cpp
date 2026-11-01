#include "audio_packet.h"
#include "audio_udp_transport.h"

#include <arpa/inet.h>
#include <cassert>
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
    assert(bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    socklen_t length = sizeof(address);
    assert(getsockname(socketFd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
    return address;
}

bool hasDatagram(int socketFd) {
    pollfd descriptor{socketFd, POLLIN, 0};
    return poll(&descriptor, 1, 100) > 0;
}
}

int main() {
    media::AudioUdpTransport transport;
    assert(transport.start());
    assert(transport.port() != 0);

    const int senderSocket = socket(AF_INET, SOCK_DGRAM, 0);
    assert(senderSocket >= 0);
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(transport.port());
    inet_pton(AF_INET, "127.0.0.1", &destination.sin_addr);

    const media::AudioPacket unauthorized{
        .sessionId = 11, .senderId = 22, .sequence = 1, .payload = {0x01}};
    const auto unauthorizedBytes = media::serializeAudioPacket(unauthorized);
    assert(sendto(senderSocket, unauthorizedBytes.data(), unauthorizedBytes.size(), 0,
                  reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) > 0);
    assert(!transport.receive(100).has_value());

    transport.registerSender(11, 22);
    const media::AudioPacket authorized{
        .sessionId = 11, .senderId = 22, .sequence = 2, .payload = {0x02, 0x03}};
    const auto authorizedBytes = media::serializeAudioPacket(authorized);
    assert(sendto(senderSocket, authorizedBytes.data(), authorizedBytes.size(), 0,
                  reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) > 0);

    const auto received = transport.receive(500);
    close(senderSocket);
    assert(received.has_value());
    assert(received->sessionId == authorized.sessionId);
    assert(received->senderId == authorized.senderId);
    assert(received->sequence == authorized.sequence);
    assert(received->payload == authorized.payload);

    const int memberSocket = socket(AF_INET, SOCK_DGRAM, 0);
    const int otherSessionSocket = socket(AF_INET, SOCK_DGRAM, 0);
    assert(memberSocket >= 0 && otherSessionSocket >= 0);
    const auto memberEndpoint = bindReceiver(memberSocket);
    const auto otherSessionEndpoint = bindReceiver(otherSessionSocket);
    transport.registerParticipant(11, 22, destination);
    transport.registerParticipant(11, 33, memberEndpoint);
    transport.registerParticipant(99, 44, otherSessionEndpoint);

    assert(transport.forward(authorized) == 1);
    assert(hasDatagram(memberSocket));
    assert(!hasDatagram(otherSessionSocket));
    close(memberSocket);
    close(otherSessionSocket);
    transport.close();

    std::cout << "Phase 5 UDP authorization test passed\n";
}
