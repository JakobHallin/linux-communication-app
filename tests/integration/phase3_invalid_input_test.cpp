#include "../support/tcp_test_support.h"

namespace {
    void expectClosed(int socketFd, const std::string& step) {
        test::require(test::waitFor([&] {
            pollfd descriptor{socketFd, POLLIN | POLLHUP, 0};
            if (poll(&descriptor, 1, 0) <= 0) return false;
            char byte = 0;
            return recv(socketFd, &byte, 1, MSG_DONTWAIT) == 0;
        }), step + ": server did not close the connection");
    }
}

int main() {
    return test::run("Phase 3 invalid input", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "Server did not start");

        {
            const test::ClientSocket invalid(test::connectClient(fixture.server.port()));
            test::require(invalid.get() >= 0, "Malformed-frame client could not connect");
            test::require(test::sendAll(invalid.get(), "BAD frame"),
                          "Could not send malformed frame fragment");
            pollfd descriptor{invalid.get(), POLLIN, 0};
            test::require(poll(&descriptor, 1, 200) == 0,
                          "Server echoed or closed an incomplete malformed frame");
            test::require(test::sendAll(invalid.get(), "\n"),
                          "Could not finish malformed frame");
            expectClosed(invalid.get(), "Malformed frame");
        }
        {
            const test::ClientSocket oversized(test::connectClient(fixture.server.port()));
            test::require(oversized.get() >= 0, "Oversized-frame client could not connect");
            test::require(test::sendAll(oversized.get(), std::string(2048, 'x')),
                          "Could not send oversized frame");
            expectClosed(oversized.get(), "Oversized frame");
        }

        const test::ClientSocket valid(test::connectClient(fixture.server.port()));
        test::require(valid.get() >= 0, "Valid client could not connect after rejections");
        test::require(test::sendAll(valid.get(), "NAME Alice\n"),
                      "Valid client could not send its name");
        test::expectLine(valid.get(), "WELCOME Alice", "Server survives invalid input");
    });
}
