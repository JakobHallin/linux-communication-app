#include "http_tcpServer.h"
#include "audio_packet.h"
#include "text_client.h"

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
    address.sin_port = 0;
    assert(bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    socklen_t length = sizeof(address);
    assert(getsockname(socketFd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
    return address;
}
}

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });
    for (int attempt = 0; attempt < 100 && server.port() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(server.port() != 0);

    client::TextClient client("127.0.0.1", server.port());
    client::TextClient otherClient("127.0.0.1", server.port());
    assert(client.connectToServer());
    assert(otherClient.connectToServer());
    assert(client.sendName("Alice"));
    assert(otherClient.sendName("Bob"));
    assert(client.receiveLine(1000) == "WELCOME Alice");
    assert(otherClient.receiveLine(1000) == "WELCOME Bob");

    const int senderSocket = socket(AF_INET, SOCK_DGRAM, 0);
    const int receiverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    assert(senderSocket >= 0 && receiverSocket >= 0);
    const auto senderEndpoint = bindUdpSocket(senderSocket);
    const auto receiverEndpoint = bindUdpSocket(receiverSocket);
    const auto ready = client.registerVoice(ntohs(senderEndpoint.sin_port), 1000);
    const auto otherReady = otherClient.registerVoice(ntohs(receiverEndpoint.sin_port), 1000);
    assert(ready.has_value());
    assert(otherReady.has_value());
    assert(ready->sessionId == 1);
    assert(otherReady->sessionId == 1);
    assert(ready->senderId != otherReady->senderId);
    assert(ready->mediaPort == server.mediaPort());
    assert(otherReady->mediaPort == server.mediaPort());

    const media::AudioPacket packet{
        .sessionId = ready->sessionId,
        .senderId = ready->senderId,
        .sequence = 0,
        .payload = {0x01, 0x02, 0x03}
    };
    const auto encoded = media::serializeAudioPacket(packet);
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddress.sin_port = htons(server.mediaPort());
    assert(sendto(senderSocket, encoded.data(), encoded.size(), 0,
                  reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) > 0);

    pollfd descriptor{receiverSocket, POLLIN, 0};
    assert(poll(&descriptor, 1, 1000) > 0);
    std::vector<std::uint8_t> received(media::maximumAudioPacketBytes);
    const auto receivedBytes = recvfrom(receiverSocket, received.data(), received.size(), 0,
                                        nullptr, nullptr);
    assert(receivedBytes > 0);
    received.resize(static_cast<std::size_t>(receivedBytes));
    const auto forwarded = media::deserializeAudioPacket(received);
    assert(forwarded.has_value());
    assert(forwarded->sessionId == packet.sessionId);
    assert(forwarded->senderId == packet.senderId);
    assert(forwarded->payload == packet.payload);

    client.close();
    otherClient.close();
    close(senderSocket);
    close(receiverSocket);
    server.closeServer();
    serverThread.join();
    std::cout << "Phase 5 voice registration integration test passed\n";
}
