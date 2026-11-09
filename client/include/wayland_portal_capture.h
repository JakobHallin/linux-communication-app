#ifndef INCLUDE_WAYLAND_PORTAL_CAPTURE_H
#define INCLUDE_WAYLAND_PORTAL_CAPTURE_H

#include <chrono>
#include <cstdint>
#include <vector>

namespace client {
    class WaylandPortalCapture {
    public:
        WaylandPortalCapture();
        ~WaylandPortalCapture();
        WaylandPortalCapture(const WaylandPortalCapture&) = delete;
        WaylandPortalCapture& operator=(const WaylandPortalCapture&) = delete;

        bool start();
        void stop();
        bool running() const;
        int pipeWireRemoteFd() const;

        struct Frame {
            std::vector<std::uint8_t> pixels;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::int32_t stride = 0;
        };

        bool readFrame(Frame& frame, std::chrono::milliseconds timeout);

        struct Impl;

    private:
        Impl* impl;
    };
}

#endif // INCLUDE_WAYLAND_PORTAL_CAPTURE_H
