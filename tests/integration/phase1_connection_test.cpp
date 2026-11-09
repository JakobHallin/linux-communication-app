#include "../support/tcp_test_support.h"

int main() {
    return test::run("Phase 1 connection", [] {
        test::RunningServer fixture;
        test::require(test::waitFor([&] { return fixture.server.port() != 0; }),
                      "Server did not start");
        test::ClientSocket first(test::connectClient(fixture.server.port()));
        test::require(first.get() >= 0, "First client could not connect");
        test::require(test::waitFor([&] {
            return fixture.server.activeConnectionCount() == 1;
        }), "First connection was not reported");
        test::ClientSocket second(test::connectClient(fixture.server.port()));
        test::require(second.get() >= 0, "Second client could not connect");
        test::require(test::waitFor([&] {
            return fixture.server.activeConnectionCount() == 2;
        }), "Second connection was not reported");

        first.reset();
        test::require(test::waitFor([&] {
            return fixture.server.activeConnectionCount() == 1;
        }), "First disconnect was not reported");
        second.reset();
        test::require(test::waitFor([&] {
            return fixture.server.activeConnectionCount() == 0;
        }), "Second disconnect was not reported");
    });
}
