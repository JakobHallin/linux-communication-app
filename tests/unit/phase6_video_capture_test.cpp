#include "video_capture_device.h"

#include <cassert>
#include <iostream>

int main() {
    client::VideoCaptureDevice unavailable("/dev/video-does-not-exist");
    assert(!unavailable.start());
    assert(!unavailable.running());
    assert(!unavailable.readFrame(0).has_value());
    unavailable.stop();

    std::cout << "Phase 6 video capture lifecycle test passed\n";
}
