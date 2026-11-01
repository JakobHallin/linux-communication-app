#include "mjpeg_frame.h"
#include "video_capture_device.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string device = argc > 1 ? argv[1] : "/dev/video0";
    const int requestedFrames = argc > 2 ? std::atoi(argv[2]) : 10;
    const bool streamMjpeg = argc > 3 && std::string(argv[3]) == "--stdout-mjpeg";
    if (requestedFrames < 0) {
        std::cerr << "Frame count must be zero or positive\n";
        return 2;
    }

    client::VideoCaptureDevice camera(device);
    if (!camera.start()) {
        std::cerr << "Could not start V4L2 camera: " << device << '\n';
        return 1;
    }

    if (!streamMjpeg) {
        std::cout << "Camera started: " << device << " " << camera.width() << 'x'
                  << camera.height() << '\n';
    }

    int captured = 0;
    for (int sequence = 0; requestedFrames == 0 || sequence < requestedFrames; ++sequence) {
        const auto frame = camera.readVideoFrame(1, 1,
                                                  static_cast<std::uint32_t>(sequence),
                                                  2000);
        if (!frame.has_value()) {
            std::cerr << "Failed to capture a valid MJPEG frame\n";
            camera.stop();
            return 1;
        }

        if (streamMjpeg) {
            std::cout.write(reinterpret_cast<const char*>(frame->payload.data()),
                            static_cast<std::streamsize>(frame->payload.size()));
            std::cout.flush();
            ++captured;
            continue;
        }

        const auto encoded = media::serializeVideoFrame(*frame);
        if (encoded.empty()) {
            std::cerr << "Failed to serialize captured video frame\n";
            camera.stop();
            return 1;
        }

        ++captured;
        std::cout << "Captured frame " << captured << ": MJPEG payload "
                  << frame->payload.size() << " bytes, envelope "
                  << encoded.size() << " bytes\n";
    }

    camera.stop();
    if (!streamMjpeg) {
        std::cout << "Video capture smoke test passed: " << captured << " frame(s)\n";
    }
    return 0;
}
