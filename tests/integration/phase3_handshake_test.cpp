#include "../support/tcp_test_support.h"

int main() {
    return test::run("Phase 3 handshake", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "Server did not start");
        const test::ClientSocket alice(test::connectClient(fixture.server.port()));
        test::require(alice.get() >= 0, "Alice could not connect");
        test::require(test::sendAll(alice.get(), "NAME Alice\n"),
                      "Alice could not send her name");
        test::expectLine(alice.get(), "WELCOME Alice", "Alice handshake");
    });
}
