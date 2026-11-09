#include "../support/tcp_test_support.h"

int main() {
    return test::run("Phase 3 handshake", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "Server did not start");
        const test::ClientSocket alice(test::connectClient(fixture.server.port()));
        test::require(alice.get() >= 0, "Alice could not connect");
        // Give the server time to process an incomplete command before sending
        // the rest. Separate writes alone need not produce separate receives.
        test::require(test::sendAll(alice.get(), "N"),
                      "Alice could not send the first command byte");
        pollfd descriptor{alice.get(), POLLIN, 0};
        test::require(poll(&descriptor, 1, 200) == 0,
                      "Server responded before the handshake command was complete");
        test::require(test::sendAll(alice.get(), "AME Al"),
                      "Alice could not send the next handshake fragment");
        descriptor.revents = 0;
        test::require(poll(&descriptor, 1, 200) == 0,
                      "Server responded before the handshake newline");
        test::require(test::sendAll(alice.get(), "ice\n"),
                      "Alice could not finish her name");
        test::expectLine(alice.get(), "WELCOME Alice", "Alice handshake");
    });
}
