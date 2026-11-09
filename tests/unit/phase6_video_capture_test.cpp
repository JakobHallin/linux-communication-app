#include "video_capture_device.h"

#include "../support/check.h"
#include <iostream>

int main() {
    client::VideoCaptureDevice unavailable("/dev/video-does-not-exist");
    TEST_CHECK(!unavailable.start());
    TEST_CHECK(!unavailable.running());
    TEST_CHECK(!unavailable.readFrame(0).has_value());
    TEST_CHECK(!unavailable.readVideoFrame(1, 2, 3, 0).has_value());
    unavailable.stop();

    std::cout << "Phase 6 video capture lifecycle test passed\n";
}
