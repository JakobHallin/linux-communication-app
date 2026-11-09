#include "wayland_portal_capture.h"

#include <libportal/portal.h>
#include <libportal/portal-helpers.h>

#include <gio/gio.h>

namespace client {
    struct WaylandPortalCapture::Impl {
        XdpPortal* portal = nullptr;
        XdpSession* session = nullptr;
        int pipeWireFd = -1;
        bool active = false;
    };

    namespace {
        struct StartOperation {
            GMainLoop* loop = nullptr;
            XdpPortal* portal = nullptr;
            XdpSession* session = nullptr;
            GError* error = nullptr;
            int pipeWireFd = -1;
            bool success = false;
        };

        void finishOperation(StartOperation* operation) {
            if (operation->loop != nullptr) g_main_loop_quit(operation->loop);
        }

        void sessionStarted(GObject*, GAsyncResult* result, gpointer userData) {
            auto* operation = static_cast<StartOperation*>(userData);
            if (!xdp_session_start_finish(operation->session, result,
                                          &operation->error)) {
                finishOperation(operation);
                return;
            }
            operation->pipeWireFd = xdp_session_open_pipewire_remote(operation->session);
            operation->success = operation->pipeWireFd >= 0;
            finishOperation(operation);
        }

        void sessionCreated(GObject*, GAsyncResult* result, gpointer userData) {
            auto* operation = static_cast<StartOperation*>(userData);
            operation->session = xdp_portal_create_screencast_session_finish(
                operation->portal, result, &operation->error);
            if (operation->session == nullptr) {
                finishOperation(operation);
                return;
            }
            xdp_session_start(operation->session, nullptr, nullptr,
                              sessionStarted, operation);
        }
    }

    WaylandPortalCapture::WaylandPortalCapture() : impl(new Impl) {}

    WaylandPortalCapture::~WaylandPortalCapture() {
        stop();
        delete impl;
    }

    bool WaylandPortalCapture::start() {
        if (impl->active) return true;
        impl->portal = xdp_portal_new();
        if (impl->portal == nullptr) return false;

        StartOperation operation;
        operation.loop = g_main_loop_new(nullptr, FALSE);
        operation.portal = impl->portal;
        xdp_portal_create_screencast_session(
            impl->portal,
            static_cast<XdpOutputType>(XDP_OUTPUT_MONITOR | XDP_OUTPUT_WINDOW),
            XDP_SCREENCAST_FLAG_NONE,
            XDP_CURSOR_MODE_EMBEDDED,
            XDP_PERSIST_MODE_TRANSIENT,
            nullptr,
            nullptr,
            sessionCreated,
            &operation);
        g_main_loop_run(operation.loop);
        g_main_loop_unref(operation.loop);

        if (!operation.success) {
            if (operation.session != nullptr) g_object_unref(operation.session);
            g_object_unref(impl->portal);
            impl->portal = nullptr;
            return false;
        }

        impl->session = operation.session;
        impl->pipeWireFd = operation.pipeWireFd;
        impl->active = true;
        return true;
    }

    void WaylandPortalCapture::stop() {
        if (impl->session != nullptr) {
            xdp_session_close(impl->session);
            g_object_unref(impl->session);
            impl->session = nullptr;
        }
        if (impl->pipeWireFd >= 0) {
            close(impl->pipeWireFd);
            impl->pipeWireFd = -1;
        }
        if (impl->portal != nullptr) {
            g_object_unref(impl->portal);
            impl->portal = nullptr;
        }
        impl->active = false;
    }

    bool WaylandPortalCapture::running() const { return impl->active; }

    int WaylandPortalCapture::pipeWireRemoteFd() const { return impl->pipeWireFd; }
}
