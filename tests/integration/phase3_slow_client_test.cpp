#include "../support/tcp_test_support.h"

int main() {
    return test::run("Phase 3 slow client", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "Server did not start");
        const test::ClientSocket sender(test::connectClient(fixture.server.port()));
        const test::ClientSocket slowClient(test::connectClient(fixture.server.port()));
        test::require(sender.get() >= 0 && slowClient.get() >= 0,
                      "Sender and slow client must both connect");
        const int receiveBuffer = 1024;
        test::require(setsockopt(slowClient.get(), SOL_SOCKET, SO_RCVBUF,
                                &receiveBuffer, sizeof(receiveBuffer)) == 0,
                      "Could not limit the slow client's receive buffer");
        test::require(test::sendAll(sender.get(), "NAME Sender\n") &&
                      test::sendAll(slowClient.get(), "NAME SlowClient\n"),
                      "Clients could not send their names");
        test::expectLine(sender.get(), "WELCOME Sender", "Sender handshake");
        test::require(test::waitFor([&] {
            return fixture.server.activeConnectionCount() == 2;
        }), "Both clients must be accepted before flooding");

        // Deliberately leave the slow client's welcome and relayed data unread.
        std::string flood;
        for (int index = 0; index < 1024; ++index) {
            flood += "MSG " + std::string(900, 'x') + " " +
                     std::to_string(index) + "\n";
        }
        test::require(test::sendAll(sender.get(), flood), "Sender could not send flood");
        test::require(test::waitFor([&] {
            return fixture.server.activeConnectionCount() == 1;
        }, std::chrono::seconds(3)), "Stalled client was not disconnected");
    });
}
