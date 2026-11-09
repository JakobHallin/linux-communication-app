#include "../support/tcp_test_support.h"

int main() {
    return test::run("Phase 3 message relay", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "Server did not start");
        const test::ClientSocket alice(test::connectClient(fixture.server.port()));
        const test::ClientSocket bob(test::connectClient(fixture.server.port()));
        test::require(alice.get() >= 0 && bob.get() >= 0,
                      "Alice and Bob must both connect");

        test::require(test::sendAll(alice.get(), "NAME Alice\n"),
                      "Alice could not send her name");
        test::expectLine(alice.get(), "WELCOME Alice", "Alice handshake");
        test::require(test::sendAll(bob.get(), "NAME Bob\n"),
                      "Bob could not send his name");
        test::expectLine(bob.get(), "WELCOME Bob", "Bob handshake");

        test::require(test::sendAll(alice.get(), "MSG hello\n"),
                      "Alice could not send a message");
        test::expectLine(bob.get(), "FROM Alice hello", "Alice to Bob relay");
        test::require(test::sendAll(bob.get(), "MSG hello back\n"),
                      "Bob could not send a reply");
        test::expectLine(alice.get(), "FROM Bob hello back", "Bob to Alice relay");
    });
}
