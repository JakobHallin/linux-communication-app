#include "../support/tcp_test_support.h"

int main() {
    return test::run("Phase 3 order/disconnect", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "Server did not start");
        test::ClientSocket alice(test::connectClient(fixture.server.port()));
        const test::ClientSocket bob(test::connectClient(fixture.server.port()));
        test::require(alice.get() >= 0 && bob.get() >= 0,
                      "Alice and Bob must both connect");
        test::require(test::sendAll(alice.get(), "NAME Alice\n") &&
                      test::sendAll(bob.get(), "NAME Bob\n"),
                      "Clients could not send their names");
        test::expectLine(alice.get(), "WELCOME Alice", "Alice handshake");
        test::expectLine(bob.get(), "WELCOME Bob", "Bob handshake");

        test::require(test::sendAll(alice.get(), "MSG first\nMSG second\n"),
                      "Alice could not send ordered messages");
        test::expectLine(bob.get(), "FROM Alice first", "First message");
        test::expectLine(bob.get(), "FROM Alice second", "Second message");

        alice.reset();
        test::require(test::waitFor([&] {
            return fixture.server.activeConnectionCount() == 1;
        }), "Alice disconnect was not reported");
        test::require(test::sendAll(bob.get(), "MSG still here\n"),
                      "Bob could not send after Alice disconnected");
    });
}
