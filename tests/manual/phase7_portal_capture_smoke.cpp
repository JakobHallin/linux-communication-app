#include "wayland_portal_capture.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    client::WaylandPortalCapture capture;
    std::cout << "Opening Wayland screen-share permission dialog...\n";
    if (!capture.start()) {
        std::cerr << "Wayland Portal screen capture was denied or unavailable\n";
        return 1;
    }

    std::cout << "Portal session active; PipeWire fd="
              << capture.pipeWireRemoteFd() << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));
    capture.stop();
    std::cout << "Portal session stopped cleanly\n";
}
