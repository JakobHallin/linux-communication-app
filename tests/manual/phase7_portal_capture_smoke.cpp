#include "wayland_portal_capture.h"

#include <iostream>
#include <chrono>

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
    capture.stop();
    std::cout << "Portal session stopped cleanly\n";
}
