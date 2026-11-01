#include "http_tcpServer.h"
#include "text_client.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });
    for (int attempt = 0; attempt < 100 && server.port() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(server.port() != 0);

    client::TextClient client("127.0.0.1", server.port());
    assert(client.connectToServer());
    assert(client.sendName("Alice"));
    assert(client.receiveLine(1000) == "WELCOME Alice");
    assert(client.sendVoicePort(9001));
    assert(client.receiveLine(1000) == "VOICE_READY 9001");

    client.close();
    server.closeServer();
    serverThread.join();
    std::cout << "Phase 5 voice registration integration test passed\n";
}
