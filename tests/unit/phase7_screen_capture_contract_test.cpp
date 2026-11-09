#include "screen_capture_source.h"

#include <cassert>
#include <iostream>

namespace {
    class FakeScreenCapture final : public client::ScreenCaptureSource {
    public:
        bool start() override {
            runningState = true;
            return true;
        }

        void stop() override { runningState = false; }

        bool running() const override { return runningState; }

        std::optional<media::VideoFrame> readFrame(
            std::uint32_t sessionId,
            std::uint32_t senderId,
            std::uint32_t sequence,
            int timeoutMilliseconds) override {
            if (!runningState || timeoutMilliseconds < 0) return std::nullopt;
            return media::VideoFrame{
                .sessionId = sessionId,
                .senderId = senderId,
                .sequence = sequence,
                .width = 2,
                .height = 2,
                .payload = {0xaa, 0xbb, 0xcc}
            };
        }

    private:
        bool runningState = false;
    };
}

int main() {
    FakeScreenCapture capture;
    assert(!capture.running());
    assert(!capture.readFrame(1, 2, 3, 0).has_value());

    assert(capture.start());
    assert(capture.running());
    const auto frame = capture.readFrame(7, 4, 9, 100);
    assert(frame.has_value());
    assert(frame->sessionId == 7);
    assert(frame->senderId == 4);
    assert(frame->sequence == 9);
    assert(frame->payload.size() == 3);

    capture.stop();
    assert(!capture.running());
    assert(!capture.readFrame(7, 4, 10, 100).has_value());

    std::cout << "Phase 7 screen capture contract test passed\n";
}
