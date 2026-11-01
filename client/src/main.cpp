#include "text_client.h"
#include "video_client.h"
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
    const bool voiceEnabled = argc > 4 && std::string(argv[4]) == "--voice";
    const bool videoEnabled = argc > 4 && std::string(argv[4]) == "--video";

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
        const std::string capture = argc > 5 ? argv[5] : "default";
        const std::string playback = argc > 6 ? argv[6] : "default";
        voice = std::make_unique<client::VoiceClient>(host, 0, 0, 0, capture, playback);
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
        const std::string device = argc > 5 ? argv[5] : "/dev/video0";
        video = std::make_unique<client::VideoClient>(host, device);
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
            std::cerr << "Failed to start webcam capture: " << device << '\n';
            video->stop();
            textClient.close();
            return 1;
        }
        std::cout << "Video enabled (session " << ready->sessionId
                  << ", sender " << ready->senderId << ")\n";
    }
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
    textClient.close();
    receiver.join();
    std::cout << "Disconnected.\n";
    return 0;
}
