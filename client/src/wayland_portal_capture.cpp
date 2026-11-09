#include "wayland_portal_capture.h"

#include <libportal/portal.h>
#include <libportal/portal-helpers.h>

#include <gio/gio.h>
#include <pipewire/pipewire.h>
#include <spa/pod/vararg.h>
#include <spa/param/format.h>
#include <spa/param/video/raw-utils.h>

#include <condition_variable>
#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <string>

namespace client {
    struct WaylandPortalCapture::Impl {
        XdpPortal* portal = nullptr;
        XdpSession* session = nullptr;
        int pipeWireFd = -1;
        bool active = false;
        pw_loop* loop = nullptr;
        pw_context* context = nullptr;
        pw_core* core = nullptr;
        pw_stream* stream = nullptr;
        spa_hook streamListener{};
        std::thread loopThread;
        std::atomic_bool loopStop{false};
        std::mutex frameMutex;
        std::condition_variable frameReady;
        Frame latestFrame;
        bool hasFrame = false;
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

    namespace {
        void streamStateChanged(void* userData, pw_stream_state,
                                pw_stream_state state, const char* error) {
            auto* impl = static_cast<WaylandPortalCapture::Impl*>(userData);
            if (error != nullptr) {
                std::cerr << "PipeWire screen stream state " << pw_stream_state_as_string(state)
                          << ": " << error << '\n';
            }
            impl->frameReady.notify_all();
        }

        void streamParamChanged(void* userData, uint32_t id, const spa_pod* param) {
            auto* impl = static_cast<WaylandPortalCapture::Impl*>(userData);
            if (id != SPA_PARAM_Format || param == nullptr) return;
            spa_video_info_raw info{};
            if (spa_format_video_raw_parse(param, &info) < 0) return;
            std::lock_guard lock(impl->frameMutex);
            impl->latestFrame.width = info.size.width;
            impl->latestFrame.height = info.size.height;
            impl->latestFrame.stride = static_cast<std::int32_t>(info.size.width * 4);
        }

        void streamProcess(void* userData) {
            auto* impl = static_cast<WaylandPortalCapture::Impl*>(userData);
            auto* buffer = pw_stream_dequeue_buffer(impl->stream);
            if (buffer == nullptr) return;
            auto* data = buffer->buffer->datas;
            if (buffer->buffer->n_datas > 0 && data != nullptr &&
                data[0].data != nullptr && data[0].chunk != nullptr &&
                data[0].chunk->size > 0) {
                const auto offset = data[0].chunk->offset;
                const auto size = data[0].chunk->size;
                if (offset <= data[0].maxsize && size <= data[0].maxsize - offset) {
                    std::lock_guard lock(impl->frameMutex);
                    const auto* begin = static_cast<const std::uint8_t*>(data[0].data) + offset;
                    impl->latestFrame.pixels.assign(begin, begin + size);
                    impl->hasFrame = true;
                    impl->frameReady.notify_all();
                }
            }
            pw_stream_queue_buffer(impl->stream, buffer);
        }

        const pw_stream_events streamEvents = {
            .version = PW_VERSION_STREAM_EVENTS,
            .state_changed = streamStateChanged,
            .param_changed = streamParamChanged,
            .process = streamProcess,
        };
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
        static bool pipeWireInitialized = false;
        if (!pipeWireInitialized) {
            pw_init(nullptr, nullptr);
            pipeWireInitialized = true;
        }
        impl->loop = pw_loop_new(nullptr);
        impl->context = impl->loop == nullptr ? nullptr : pw_context_new(impl->loop, nullptr, 0);
        impl->core = impl->context == nullptr ? nullptr :
            pw_context_connect_fd(impl->context, impl->pipeWireFd, nullptr, 0);
        if (impl->core == nullptr) {
            stop();
            return false;
        }
        guint32 nodeId = PW_ID_ANY;
        if (auto* streams = xdp_session_get_streams(impl->session); streams != nullptr) {
            GVariantIter iter;
            guint32 id = 0;
            GVariant* properties = nullptr;
            g_variant_iter_init(&iter, streams);
            if (g_variant_iter_next(&iter, "(u@a{sv})", &id, &properties)) {
                nodeId = id;
                g_variant_unref(properties);
            }
            g_variant_unref(streams);
        }
        auto* streamProperties = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Video",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Screen",
            nullptr);
        const auto target = std::to_string(nodeId);
        pw_properties_set(streamProperties, PW_KEY_TARGET_OBJECT, target.c_str());
        impl->stream = pw_stream_new(impl->core, "communication-app screen capture",
                                      streamProperties);
        if (impl->stream == nullptr) {
            stop();
            return false;
        }
        pw_stream_add_listener(impl->stream, &impl->streamListener, &streamEvents, impl);
        std::uint8_t podBuffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
        const spa_rectangle size{1920, 1080};
        const spa_fraction rate{30, 1};
        const auto* format = static_cast<const spa_pod*>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format,
            SPA_POD_CHOICE_ENUM_Id(4, SPA_VIDEO_FORMAT_BGRx,
                                   SPA_VIDEO_FORMAT_BGRA,
                                   SPA_VIDEO_FORMAT_RGBx,
                                   SPA_VIDEO_FORMAT_RGBA),
            SPA_FORMAT_VIDEO_size, SPA_POD_Rectangle(&size),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_Fraction(&rate)));
        if (pw_stream_connect(impl->stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                              static_cast<pw_stream_flags>(
                                  PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                              &format, 1) < 0) {
            stop();
            return false;
        }
        impl->active = true;
        impl->loopStop = false;
        impl->loopThread = std::thread([this] {
            pw_loop_enter(impl->loop);
            pw_stream_set_active(impl->stream, true);
            while (!impl->loopStop.load()) {
                pw_loop_iterate(impl->loop, 100);
            }
            pw_loop_leave(impl->loop);
        });
        return true;
    }

    void WaylandPortalCapture::stop() {
        impl->active = false;
        impl->frameReady.notify_all();
        impl->loopStop = true;
        if (impl->loopThread.joinable()) impl->loopThread.join();
        if (impl->stream != nullptr) {
            pw_stream_destroy(impl->stream);
            impl->stream = nullptr;
        }
        if (impl->core != nullptr) {
            pw_core_disconnect(impl->core);
            impl->core = nullptr;
        }
        if (impl->context != nullptr) {
            pw_context_destroy(impl->context);
            impl->context = nullptr;
        }
        if (impl->loop != nullptr) {
            pw_loop_destroy(impl->loop);
            impl->loop = nullptr;
        }
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

    bool WaylandPortalCapture::readFrame(Frame& frame,
                                         std::chrono::milliseconds timeout) {
        std::unique_lock lock(impl->frameMutex);
        if (!impl->frameReady.wait_for(lock, timeout,
                                       [this] { return impl->hasFrame || !impl->active; })) {
            return false;
        }
        if (!impl->hasFrame) return false;
        frame = impl->latestFrame;
        impl->hasFrame = false;
        return true;
    }
}
