#include "wayland_portal_capture.h"

#include <libportal/portal.h>
#include <libportal/portal-helpers.h>

#include <gio/gio.h>
#include <pipewire/pipewire.h>
#include <pipewire/core.h>
#include <pipewire/thread-loop.h>
#include <spa/pod/vararg.h>
#include <spa/param/buffers.h>
#include <spa/param/format.h>
#include <spa/param/video/raw-utils.h>

#include <condition_variable>
#include <array>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>

namespace client {
    struct WaylandPortalCapture::Impl {
        XdpPortal* portal = nullptr;
        XdpSession* session = nullptr;
        int pipeWireFd = -1;
        bool active = false;
        pw_thread_loop* threadLoop = nullptr;
        pw_loop* loop = nullptr;
        bool loopStarted = false;
        pw_context* context = nullptr;
        pw_core* core = nullptr;
        spa_hook coreListener{};
        pw_registry* registry = nullptr;
        spa_hook registryListener{};
        pw_stream* stream = nullptr;
        spa_hook streamListener{};
        std::mutex frameMutex;
        std::condition_variable frameReady;
        Frame latestFrame;
        bool hasFrame = false;
        std::array<std::uint8_t, 2048> formatBuffer{};
        spa_rectangle preferredSize{1920, 1080};
        spa_rectangle minimumSize{1, 1};
        spa_rectangle maximumSize{4096, 4096};
        spa_fraction preferredRate{30, 1};
        spa_fraction minimumRate{0, 1};
        spa_fraction maximumRate{1000, 1};
        std::mutex targetMutex;
        std::condition_variable targetReady;
        std::string targetObject;
        uint32_t targetNodeId = PW_ID_ANY;
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
        void registryGlobal(void* userData, uint32_t id, uint32_t,
                            const char* type, uint32_t,
                            const spa_dict* properties) {
            auto* impl = static_cast<WaylandPortalCapture::Impl*>(userData);
            if (type == nullptr || properties == nullptr ||
                std::string(type) != PW_TYPE_INTERFACE_Node) {
                return;
            }
            if (id != impl->targetNodeId) return;
            const auto* serial = spa_dict_lookup(properties, PW_KEY_OBJECT_SERIAL);
            const auto* nodeName = spa_dict_lookup(properties, PW_KEY_NODE_NAME);
            if (serial == nullptr && nodeName == nullptr) return;
            std::lock_guard lock(impl->targetMutex);
            impl->targetObject = serial != nullptr ? serial : nodeName;
            impl->targetReady.notify_all();
        }

        void registryGlobalRemove(void*, uint32_t) {}

        const pw_registry_events registryEvents = {
            .version = PW_VERSION_REGISTRY_EVENTS,
            .global = registryGlobal,
            .global_remove = registryGlobalRemove,
        };

        void coreError(void*, uint32_t id, int seq, int res, const char* message) {
            std::cerr << "PipeWire core error (id=" << id << ", seq=" << seq
                      << ", code=" << res << "): "
                      << (message != nullptr ? message : "unknown") << '\n';
        }

        const pw_core_events coreEvents = {
            .version = PW_VERSION_CORE_EVENTS,
            .error = coreError,
        };

        void streamStateChanged(void* userData, pw_stream_state oldState,
                                pw_stream_state state, const char* error) {
            auto* impl = static_cast<WaylandPortalCapture::Impl*>(userData);
            std::cerr << "PipeWire screen stream state "
                      << pw_stream_state_as_string(oldState) << " -> "
                      << pw_stream_state_as_string(state) << '\n';
            if (error != nullptr) {
                std::cerr << "PipeWire screen stream error: " << error << '\n';
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

            // The portal stream stays PAUSED until the consumer advertises
            // which buffer storage it can accept.  Request ordinary mapped
            // memory so streamProcess can copy the pixels safely.
            std::array<std::uint8_t, 256> bufferParams{};
            spa_pod_builder builder = SPA_POD_BUILDER_INIT(
                bufferParams.data(), static_cast<uint32_t>(bufferParams.size()));
            const spa_pod* params[] = {
                static_cast<const spa_pod*>(spa_pod_builder_add_object(
                    &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                    SPA_PARAM_BUFFERS_dataType,
                    SPA_POD_Int(1 << SPA_DATA_MemPtr)))
            };
            pw_stream_update_params(impl->stream, params, 1);
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
        impl->threadLoop = pw_thread_loop_new("communication-app-screen-capture", nullptr);
        impl->loop = impl->threadLoop == nullptr ? nullptr :
            pw_thread_loop_get_loop(impl->threadLoop);
        impl->context = impl->loop == nullptr ? nullptr : pw_context_new(impl->loop, nullptr, 0);
        impl->core = impl->context == nullptr ? nullptr :
            pw_context_connect_fd(impl->context, impl->pipeWireFd, nullptr, 0);
        if (impl->core == nullptr) {
            stop();
            return false;
        }
        pw_core_add_listener(impl->core, &impl->coreListener,
                             &coreEvents, impl);
        guint32 nodeId = PW_ID_ANY;
        guint64 pipeWireSerial = 0;
        if (auto* streams = xdp_session_get_streams(impl->session); streams != nullptr) {
            GVariantIter iter;
            guint32 id = 0;
            GVariant* properties = nullptr;
            g_variant_iter_init(&iter, streams);
            if (g_variant_iter_next(&iter, "(u@a{sv})", &id, &properties)) {
                nodeId = id;
                // Since ScreenCast portal version 6, this stable serial is
                // preferred over the reusable PipeWire node ID.
                g_variant_lookup(properties, "pipewire-serial", "t", &pipeWireSerial);
                g_variant_unref(properties);
            }
            g_variant_unref(streams);
        }
        if (nodeId == PW_ID_ANY) {
            stop();
            return false;
        }
        impl->targetNodeId = nodeId;

        impl->registry = pw_core_get_registry(impl->core, PW_VERSION_REGISTRY,
                                               0);
        if (impl->registry == nullptr ||
            pw_registry_add_listener(impl->registry, &impl->registryListener,
                                     &registryEvents, impl) < 0) {
            stop();
            return false;
        }
        if (pw_thread_loop_start(impl->threadLoop) < 0) {
            stop();
            return false;
        }
        impl->loopStarted = true;
        {
            std::unique_lock lock(impl->targetMutex);
            impl->targetReady.wait_for(lock, std::chrono::seconds(2),
                                       [capture = impl] {
                                           return !capture->targetObject.empty();
                                       });
        }
        auto* streamProperties = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Video",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Screen",
            nullptr);
        pw_thread_loop_lock(impl->threadLoop);
        uint32_t connectTarget = PW_ID_ANY;
        if (pipeWireSerial != 0) {
            const auto target = std::to_string(pipeWireSerial);
            pw_properties_set(streamProperties, PW_KEY_TARGET_OBJECT, target.c_str());
            std::cerr << "PipeWire screen target: " << target << " (serial)\n";
        } else if (!impl->targetObject.empty()) {
            // Older portals omit pipewire-serial from Start(), but the
            // private registry still exposes object.serial for the node.
            pw_properties_set(streamProperties, PW_KEY_TARGET_OBJECT,
                              impl->targetObject.c_str());
            std::cerr << "PipeWire screen target: " << impl->targetObject
                      << " (registry property)\n";
        } else {
            // Older portals expose only the node ID.  PipeWire clients such
            // as GNOME's screen-capture backend use that ID as target_id.
            connectTarget = nodeId;
            std::cerr << "PipeWire screen target: " << nodeId
                      << " (legacy node ID)\n";
        }
        impl->stream = pw_stream_new(impl->core, "communication-app screen capture",
                                      streamProperties);
        if (impl->stream == nullptr) {
            pw_thread_loop_unlock(impl->threadLoop);
            stop();
            return false;
        }
        pw_stream_add_listener(impl->stream, &impl->streamListener, &streamEvents, impl);
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(
            impl->formatBuffer.data(),
            static_cast<uint32_t>(impl->formatBuffer.size()));
        const auto* format = static_cast<const spa_pod*>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format,
            SPA_POD_CHOICE_ENUM_Id(6, SPA_VIDEO_FORMAT_BGRx,
                                   SPA_VIDEO_FORMAT_BGRx,
                                   SPA_VIDEO_FORMAT_BGRA,
                                   SPA_VIDEO_FORMAT_RGBx,
                                   SPA_VIDEO_FORMAT_RGBA,
                                   SPA_VIDEO_FORMAT_RGB),
            SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(
                &impl->preferredSize, &impl->minimumSize, &impl->maximumSize),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
                &impl->preferredRate, &impl->minimumRate, &impl->maximumRate)));
        if (pw_stream_connect(impl->stream, PW_DIRECTION_INPUT, connectTarget,
                              static_cast<pw_stream_flags>(
                                  PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                              &format, 1) < 0) {
            pw_thread_loop_unlock(impl->threadLoop);
            stop();
            return false;
        }
        pw_thread_loop_unlock(impl->threadLoop);
        impl->active = true;
        return true;
    }

    void WaylandPortalCapture::stop() {
        impl->active = false;
        impl->frameReady.notify_all();
        if (impl->threadLoop != nullptr && impl->loopStarted) {
            pw_thread_loop_stop(impl->threadLoop);
            impl->loopStarted = false;
        }
        if (impl->stream != nullptr) {
            pw_stream_destroy(impl->stream);
            impl->stream = nullptr;
        }
        if (impl->registry != nullptr) {
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(impl->registry));
            impl->registry = nullptr;
        }
        if (impl->core != nullptr) {
            pw_core_disconnect(impl->core);
            impl->core = nullptr;
        }
        if (impl->context != nullptr) {
            pw_context_destroy(impl->context);
            impl->context = nullptr;
        }
        if (impl->threadLoop != nullptr) {
            pw_thread_loop_destroy(impl->threadLoop);
            impl->threadLoop = nullptr;
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
