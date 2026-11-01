#include "video_client.h"

#include "video_capture_device.h"
#include "video_frame.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <utility>

namespace client {
    struct VideoClient::Impl {
        explicit Impl(std::string path) : capture(std::move(path)) {}
        VideoCaptureDevice capture;
    };

    VideoClient::VideoClient(std::string host, std::string device)
        : serverHost(std::move(host)), captureDevice(std::move(device)),
          impl(new Impl(captureDevice)) {}

    VideoClient::~VideoClient() {
        stop();
        delete impl;
    }

    bool VideoClient::start() {
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

    bool VideoClient::activate() {
        if (socketFd < 0 || mediaPort == 0 || sessionId == 0 || senderId == 0 ||
            running.load()) {
            return false;
        }
        if (!impl->capture.start()) return false;
        running = true;
        captureThread = std::thread(&VideoClient::captureLoop, this);
        return true;
    }

    void VideoClient::stop() {
        running = false;
        if (impl != nullptr) impl->capture.stop();
        if (captureThread.joinable()) captureThread.join();
        if (socketFd >= 0) {
            ::close(socketFd);
            socketFd = -1;
        }
        boundPort = 0;
    }

    std::uint16_t VideoClient::localPort() const { return boundPort; }

    void VideoClient::setMediaPort(std::uint16_t port) { mediaPort = port; }

    void VideoClient::setIdentity(std::uint32_t session, std::uint32_t sender) {
        sessionId = session;
        senderId = sender;
    }

    void VideoClient::captureLoop() {
        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_port = htons(mediaPort);
        if (inet_pton(AF_INET, serverHost.c_str(), &destination.sin_addr) != 1) {
            std::cerr << "Video server host must be an IPv4 address\n";
            running = false;
            return;
        }

        std::uint32_t sequence = 0;
        while (running) {
            const auto frame = impl->capture.readVideoFrame(
                sessionId, senderId, sequence++, 200);
            if (!frame.has_value()) continue;
            const auto bytes = media::serializeVideoFrame(*frame);
            if (bytes.empty()) continue;
            const auto sent = sendto(socketFd, bytes.data(), bytes.size(), 0,
                                     reinterpret_cast<sockaddr*>(&destination),
                                     sizeof(destination));
            if (sent != static_cast<ssize_t>(bytes.size())) {
                std::cerr << "Video frame send failed\n";
                break;
            }
        }
        running = false;
    }
}
