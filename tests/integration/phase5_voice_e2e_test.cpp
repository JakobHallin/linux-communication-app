#include "audio_codec.h"
#include "audio_packet.h"
#include "audio_stream_mixer.h"
#include "audio_udp_transport.h"

#include <arpa/inet.h>
#include "../support/check.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
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

std::vector<std::uint8_t> receiveDatagram(int socketFd) {
    pollfd descriptor{socketFd, POLLIN, 0};
    TEST_CHECK(poll(&descriptor, 1, 500) > 0);
    std::vector<std::uint8_t> bytes(media::maximumAudioPacketBytes);
    const auto received = recvfrom(socketFd, bytes.data(), bytes.size(), 0, nullptr, nullptr);
    TEST_CHECK(received > 0);
    bytes.resize(static_cast<std::size_t>(received));
    return bytes;
}
}

int main() {
    media::AudioUdpTransport server;
    TEST_CHECK(server.start());

    const int senderSocket = socket(AF_INET, SOCK_DGRAM, 0);
    const int receiverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_CHECK(senderSocket >= 0 && receiverSocket >= 0);
    const auto senderEndpoint = bindUdpSocket(senderSocket);
    const auto receiverEndpoint = bindUdpSocket(receiverSocket);

    constexpr std::uint32_t sessionId = 123;
    constexpr std::uint32_t senderId = 1;
    constexpr std::uint32_t receiverId = 2;
    server.registerParticipant(sessionId, senderId, senderEndpoint);
    server.registerParticipant(sessionId, receiverId, receiverEndpoint);

    media::OpusCodec codec;
    std::vector<std::int16_t> samples(media::OpusCodec::frameSamples);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = static_cast<std::int16_t>(
            5000.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * index /
                               media::OpusCodec::sampleRate));
    }
    const auto encoded = codec.encode(samples);
    TEST_CHECK(!encoded.empty());

    const media::AudioPacket outgoing{
        .sessionId = sessionId,
        .senderId = senderId,
        .sequence = 1,
        .payload = encoded
    };
    const auto bytes = media::serializeAudioPacket(outgoing);
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddress.sin_port = htons(server.port());
    TEST_CHECK(sendto(senderSocket, bytes.data(), bytes.size(), 0,
                  reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) > 0);

    const auto received = server.receive(500);
    TEST_CHECK(received.has_value());
    TEST_CHECK(server.forward(*received) == 1);

    const auto forwardedBytes = receiveDatagram(receiverSocket);
    const auto forwarded = media::deserializeAudioPacket(forwardedBytes);
    TEST_CHECK(forwarded.has_value());
    TEST_CHECK(forwarded->sessionId == sessionId);
    TEST_CHECK(forwarded->senderId == senderId);
    TEST_CHECK(forwarded->sequence == 1);

    const auto decoded = codec.decode(forwarded->payload);
    TEST_CHECK(decoded.has_value());
    TEST_CHECK(decoded->size() == media::OpusCodec::frameSamples);
    media::AudioStreamMixer mixer;
    TEST_CHECK(mixer.submit(senderId, *decoded));
    TEST_CHECK(!mixer.mix().empty());

    close(senderSocket);
    close(receiverSocket);
    server.close();
    std::cout << "Phase 5 voice end-to-end test passed\n";
}
