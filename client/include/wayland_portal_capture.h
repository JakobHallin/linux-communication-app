#ifndef INCLUDE_WAYLAND_PORTAL_CAPTURE_H
#define INCLUDE_WAYLAND_PORTAL_CAPTURE_H

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

    private:
        struct Impl;
        Impl* impl;
    };
}

#endif // INCLUDE_WAYLAND_PORTAL_CAPTURE_H
