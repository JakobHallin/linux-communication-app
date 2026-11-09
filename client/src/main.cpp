#include "text_client.h"
#include "video_client.h"
#ifdef ENABLE_WAYLAND_SCREEN_CAPTURE
#include "screen_video_client.h"
#endif
#ifdef ENABLE_LIVE_AUDIO_CLIENT
#include "voice_client.h"
#endif

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = argc > 2 ? static_cast<std::uint16_t>(std::stoul(argv[2])) : 8080;
    const std::string name = argc > 3 ? argv[3] : "Client";
    bool voiceEnabled = false;
    bool videoEnabled = false;
    bool screenShareEnabled = false;
    std::string voiceCapture = "default";
    std::string voicePlayback = "default";
    std::string videoDevice = "/dev/video0";
    for (int argument = 4; argument < argc; ++argument) {
        const std::string option = argv[argument];
        if (option == "--voice") {
            voiceEnabled = true;
            if (argument + 1 < argc && std::string(argv[argument + 1]).rfind("--", 0) != 0) {
                voiceCapture = argv[++argument];
            }
            if (argument + 1 < argc && std::string(argv[argument + 1]).rfind("--", 0) != 0) {
                voicePlayback = argv[++argument];
            }
        } else if (option == "--video") {
            videoEnabled = true;
            if (argument + 1 < argc && std::string(argv[argument + 1]).rfind("--", 0) != 0) {
                videoDevice = argv[++argument];
            }
        } else if (option == "--screen-share") {
            screenShareEnabled = true;
        }
    }

    if (videoEnabled && screenShareEnabled) {
        std::cerr << "Choose either --video or --screen-share, not both\n";
        return 1;
    }

    client::TextClient textClient(host, port);
    if (!textClient.connectToServer()) {
        std::cerr << "Failed to connect to " << host << ':' << port << '\n';
        return 1;
    }
    if (!textClient.sendName(name)) {
        std::cerr << "Failed to send display name\n";
        textClient.close();
        return 1;
    }
    const auto welcome = textClient.receiveLine(1000);
    if (!welcome.has_value()) {
        std::cerr << "Server did not acknowledge the display name\n";
        textClient.close();
        return 1;
    }

    std::cout << *welcome << '\n'
              << "Connected. Type messages; use /quit to disconnect.\n";
#ifdef ENABLE_LIVE_AUDIO_CLIENT
    std::unique_ptr<client::VoiceClient> voice;
    if (voiceEnabled) {
        voice = std::make_unique<client::VoiceClient>(
            host, 0, 0, 0, voiceCapture, voicePlayback);
        if (!voice->start()) {
            std::cerr << "Failed to start voice UDP socket\n";
            textClient.close();
            return 1;
        }
        const auto ready = textClient.registerVoice(voice->localPort(), 1000);
        if (!ready.has_value()) {
            std::cerr << "Server did not acknowledge voice registration\n";
            voice->stop();
            textClient.close();
            return 1;
        }
        voice->setMediaPort(ready->mediaPort);
        voice->setIdentity(ready->sessionId, ready->senderId);
        voice->activate();
        std::cout << "Voice enabled (session " << ready->sessionId
                  << ", sender " << ready->senderId << ")\n";
    }
#else
    if (voiceEnabled) {
        std::cerr << "Voice support is disabled; rebuild with ENABLE_LIVE_AUDIO_CLIENT=ON\n";
        textClient.close();
        return 1;
    }
#endif
    std::unique_ptr<client::VideoClient> video;
    if (videoEnabled) {
        video = std::make_unique<client::VideoClient>(host, videoDevice);
        if (!video->start()) {
            std::cerr << "Failed to start video UDP socket\n";
            textClient.close();
            return 1;
        }
        const auto ready = textClient.registerVideo(video->localPort(), 1000);
        if (!ready.has_value()) {
            std::cerr << "Server did not acknowledge video registration\n";
            video->stop();
            textClient.close();
            return 1;
        }
        video->setMediaPort(ready->mediaPort);
        video->setIdentity(ready->sessionId, ready->senderId);
        if (!video->activate()) {
            std::cerr << "Failed to start webcam capture: " << videoDevice << '\n';
            video->stop();
            textClient.close();
            return 1;
        }
        std::cout << "Video enabled (session " << ready->sessionId
                  << ", sender " << ready->senderId << ")\n";
    }
#ifdef ENABLE_WAYLAND_SCREEN_CAPTURE
    std::unique_ptr<client::ScreenVideoClient> screenVideo;
    if (screenShareEnabled) {
        screenVideo = std::make_unique<client::ScreenVideoClient>(host);
        if (!screenVideo->start()) {
            std::cerr << "Failed to start screen-share UDP socket\n";
            textClient.close();
            return 1;
        }
        const auto ready = textClient.registerVideo(screenVideo->localPort(), 1000);
        if (!ready.has_value()) {
            std::cerr << "Server did not acknowledge screen-share registration\n";
            screenVideo->stop();
            textClient.close();
            return 1;
        }
        screenVideo->setMediaPort(ready->mediaPort);
        screenVideo->setIdentity(ready->sessionId, ready->senderId);
        if (!screenVideo->activate()) {
            std::cerr << "Failed to start Wayland screen capture\n";
            screenVideo->stop();
            textClient.close();
            return 1;
        }
        std::cout << "Screen sharing enabled (session " << ready->sessionId
                  << ", sender " << ready->senderId << ")\n";
    }
#else
    if (screenShareEnabled) {
        std::cerr << "Screen sharing is disabled; rebuild with "
                     "ENABLE_WAYLAND_SCREEN_CAPTURE=ON\n";
        textClient.close();
        return 1;
    }
#endif
    std::atomic_bool running{true};
    std::thread receiver([&] {
        while (running) {
            const auto line = textClient.receiveLine(250);
            if (line.has_value()) {
                std::cout << *line << '\n';
            }
        }
    });

    std::string line;
#ifdef ENABLE_LIVE_AUDIO_CLIENT
    bool voiceMuted = false;
#endif
    while (std::getline(std::cin, line) && line != "/quit") {
#ifdef ENABLE_LIVE_AUDIO_CLIENT
        if (line == "/mute" && voice) {
            voiceMuted = !voiceMuted;
            voice->setMuted(voiceMuted);
            std::cout << (voiceMuted ? "Microphone muted.\n" : "Microphone unmuted.\n");
            continue;
        }
#endif
        if (!line.empty() && !textClient.sendMessage(line)) {
            std::cerr << "Failed to send message\n";
            break;
        }
    }
    running = false;
#ifdef ENABLE_LIVE_AUDIO_CLIENT
    if (voice) voice->stop();
#endif
    if (video) video->stop();
#ifdef ENABLE_WAYLAND_SCREEN_CAPTURE
    if (screenVideo) screenVideo->stop();
#endif
    textClient.close();
    receiver.join();
    std::cout << "Disconnected.\n";
    return 0;
}
