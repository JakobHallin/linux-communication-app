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

    client::TextClient alice("127.0.0.1", server.port());
    client::TextClient bob("127.0.0.1", server.port());
    assert(alice.connectToServer());
    assert(bob.connectToServer());
    assert(alice.sendName("Alice"));
    assert(bob.sendName("Bob"));
    assert(alice.receiveLine(1000) == "WELCOME Alice");
    assert(bob.receiveLine(1000) == "WELCOME Bob");

    assert(alice.sendMessage("voice client is still text compatible"));
    assert(bob.receiveLine(1000) ==
           "FROM Alice voice client is still text compatible");

    alice.close();
    bob.close();
    server.closeServer();
    serverThread.join();
    std::cout << "Phase 5 unified text client test passed\n";
}
