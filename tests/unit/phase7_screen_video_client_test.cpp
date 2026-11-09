#include "screen_video_client.h"

#include "../support/check.h"
#include <iostream>

int main() {
    client::ScreenVideoClient client("127.0.0.1");
    TEST_CHECK(client.start());
    TEST_CHECK(client.localPort() != 0);
    client.setMediaPort(40000);
    client.setIdentity(1, 2);
    client.stop();
    TEST_CHECK(client.localPort() == 0);

    std::cout << "Phase 7 screen video client test passed\n";
}
