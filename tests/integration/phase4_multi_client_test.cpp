#include "../support/tcp_test_support.h"

#include <array>

int main() {
    return test::run("Phase 4 multi-client", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "Server did not start");
        const std::array<test::ClientSocket, 3> clients{
            test::ClientSocket(test::connectClient(fixture.server.port())),
            test::ClientSocket(test::connectClient(fixture.server.port())),
            test::ClientSocket(test::connectClient(fixture.server.port()))
        };
        for (std::size_t index = 0; index < clients.size(); ++index) {
            const auto name = "Client" + std::to_string(index + 1);
            test::require(clients[index].get() >= 0, name + " could not connect");
            test::require(test::sendAll(clients[index].get(), "NAME " + name + "\n"),
                          name + " could not send its name");
            test::expectLine(clients[index].get(), "WELCOME " + name, name + " handshake");
        }

        test::require(test::sendAll(clients[0].get(), "MSG from-one\n"),
                      "Client1 could not send");
        test::expectLine(clients[1].get(), "FROM Client1 from-one", "Client1 to Client2");
        test::expectLine(clients[2].get(), "FROM Client1 from-one", "Client1 to Client3");

        test::require(test::sendAll(clients[1].get(), "MSG from-two\n"),
                      "Client2 could not send");
        test::expectLine(clients[0].get(), "FROM Client2 from-two", "Client2 to Client1");
        test::expectLine(clients[2].get(), "FROM Client2 from-two", "Client2 to Client3");
    });
}
