#include "../support/tcp_test_support.h"

int main() {
    return test::run("Phase 2 asynchronous round trip", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "io_uring server did not start");
        const test::ClientSocket client(test::connectClient(fixture.server.port()));
        test::require(client.get() >= 0, "Client could not connect");

        // Use today's line protocol rather than the obsolete raw-echo protocol.
        // Separate writes exercise the stream parser without assuming TCP
        // preserves write boundaries or guarantees separate receive completions.
        test::require(test::sendAll(client.get(), "NAME io_"),
                      "Could not send the first handshake fragment");
        test::require(test::sendAll(client.get(), "uring\n"),
                      "Could not send the final handshake fragment");
        test::expectLine(client.get(), "WELCOME io_uring", "Asynchronous response");
    });
}
