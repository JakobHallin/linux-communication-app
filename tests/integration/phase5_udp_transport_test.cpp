#include "audio_packet.h"
#include "audio_udp_transport.h"

#include <arpa/inet.h>
#include <cassert>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

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
    transport.close();
    assert(received.has_value());
    assert(received->sessionId == authorized.sessionId);
    assert(received->senderId == authorized.senderId);
    assert(received->sequence == authorized.sequence);
    assert(received->payload == authorized.payload);
    std::cout << "Phase 5 UDP authorization test passed\n";
}
