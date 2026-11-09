#include "http_tcpServer.h"
#include "text_client.h"

#include "../support/check.h"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    http::TcpServer server(0);
    std::thread serverThread([&server] { server.start(); });
    for (int attempt = 0; attempt < 100 && server.port() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_CHECK(server.port() != 0);

    client::TextClient alice("127.0.0.1", server.port());
    client::TextClient bob("127.0.0.1", server.port());
    TEST_CHECK(alice.connectToServer());
    TEST_CHECK(bob.connectToServer());
    TEST_CHECK(alice.sendName("Alice"));
    TEST_CHECK(bob.sendName("Bob"));
    TEST_CHECK(alice.receiveLine(1000) == "WELCOME Alice");
    TEST_CHECK(bob.receiveLine(1000) == "WELCOME Bob");

    TEST_CHECK(alice.sendMessage("voice client is still text compatible"));
    TEST_CHECK(bob.receiveLine(1000) ==
           "FROM Alice voice client is still text compatible");

    alice.close();
    bob.close();
    server.closeServer();
    serverThread.join();
    std::cout << "Phase 5 unified text client test passed\n";
}
