#include "wayland_portal_capture.h"
#include "screen_frame_encoder.h"

#include <iostream>
#include <chrono>
#include <cstdint>
#include <utility>

int main() {
    client::WaylandPortalCapture capture;
    std::cout << "Opening Wayland screen-share permission dialog...\n";
    if (!capture.start()) {
        std::cerr << "Wayland Portal screen capture was denied or unavailable\n";
        return 1;
    }

    std::cout << "Portal session active; PipeWire fd="
              << capture.pipeWireRemoteFd() << "\n";
    client::WaylandPortalCapture::Frame frame;
    if (!capture.readFrame(frame, std::chrono::seconds(10))) {
        std::cerr << "Portal session started, but no PipeWire video frame arrived\n";
        return 1;
    }
    std::cout << "Received screen frame: " << frame.width << 'x' << frame.height
              << ", " << frame.pixels.size() << " bytes\n";
    const media::BgraFrame rawFrame{
        .width = static_cast<std::uint16_t>(frame.width),
        .height = static_cast<std::uint16_t>(frame.height),
        .stride = frame.stride,
        .pixels = std::move(frame.pixels)
    };
    const auto encoded = media::makeMjpegVideoFrameFromBgra(1, 1, 0, rawFrame);
    if (!encoded.has_value()) {
        std::cerr << "Captured screen frame could not be encoded as MJPEG\n";
        return 1;
    }
    std::cout << "Encoded screen frame as MJPEG: " << encoded->payload.size()
              << " bytes\n";
    capture.stop();
    std::cout << "Portal session stopped cleanly\n";
}
