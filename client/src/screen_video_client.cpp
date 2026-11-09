#include "screen_video_client.h"

#include "screen_frame_encoder.h"
#include "video_frame.h"
#include "wayland_portal_capture.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <chrono>
#include <climits>
#include <utility>

namespace client {
    struct ScreenVideoClient::Impl {
        explicit Impl() = default;
        WaylandPortalCapture capture;
    };

    ScreenVideoClient::ScreenVideoClient(std::string host)
        : serverHost(std::move(host)), impl(new Impl) {}

    ScreenVideoClient::~ScreenVideoClient() {
        stop();
        delete impl;
    }

    bool ScreenVideoClient::start() {
        if (socketFd >= 0) return true;
        socketFd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socketFd < 0) return false;

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(0);
        if (bind(socketFd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
            stop();
            return false;
        }

        socklen_t length = sizeof(local);
        if (getsockname(socketFd, reinterpret_cast<sockaddr*>(&local), &length) < 0) {
            stop();
            return false;
        }
        boundPort = ntohs(local.sin_port);
        return true;
    }

    bool ScreenVideoClient::activate() {
        if (socketFd < 0 || mediaPort == 0 || sessionId == 0 || senderId == 0 ||
            running.load()) {
            return false;
        }
        if (!impl->capture.start()) return false;
        running = true;
        captureThread = std::thread(&ScreenVideoClient::captureLoop, this);
        return true;
    }

    void ScreenVideoClient::stop() {
        running = false;
        if (impl != nullptr) impl->capture.stop();
        if (captureThread.joinable()) captureThread.join();
        if (socketFd >= 0) {
            ::close(socketFd);
            socketFd = -1;
        }
        boundPort = 0;
    }

    std::uint16_t ScreenVideoClient::localPort() const { return boundPort; }

    void ScreenVideoClient::setMediaPort(std::uint16_t port) { mediaPort = port; }

    void ScreenVideoClient::setIdentity(std::uint32_t session, std::uint32_t sender) {
        sessionId = session;
        senderId = sender;
    }

    void ScreenVideoClient::captureLoop() {
        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_port = htons(mediaPort);
        if (inet_pton(AF_INET, serverHost.c_str(), &destination.sin_addr) != 1) {
            std::cerr << "Screen video server host must be an IPv4 address\n";
            running = false;
            return;
        }

        std::uint32_t sequence = 0;
        while (running) {
            WaylandPortalCapture::Frame captured;
            if (!impl->capture.readFrame(captured, std::chrono::milliseconds(200))) continue;
            if (captured.width > UINT16_MAX || captured.height > UINT16_MAX ||
                captured.stride <= 0) continue;

            const media::BgraFrame raw{
                .width = static_cast<std::uint16_t>(captured.width),
                .height = static_cast<std::uint16_t>(captured.height),
                .stride = captured.stride,
                .pixels = std::move(captured.pixels)
            };
            const auto frame = media::makeMjpegVideoFrameFromBgra(
                sessionId, senderId, sequence++, raw);
            if (!frame.has_value()) continue;
            const auto bytes = media::serializeVideoFrame(*frame);
            if (bytes.empty()) continue;
            const auto sent = sendto(socketFd, bytes.data(), bytes.size(), 0,
                                     reinterpret_cast<sockaddr*>(&destination),
                                     sizeof(destination));
            if (sent != static_cast<ssize_t>(bytes.size())) {
                std::cerr << "Screen video frame send failed\n";
                break;
            }
        }
        running = false;
    }
}
