#include "video_client.h"

#include "../support/check.h"
#include <iostream>

int main() {
    client::VideoClient client("127.0.0.1", "/dev/video-does-not-exist");
    TEST_CHECK(client.start());
    TEST_CHECK(client.localPort() != 0);
    client.setMediaPort(40000);
    client.setIdentity(1, 2);
    TEST_CHECK(!client.activate());
    client.stop();
    TEST_CHECK(client.localPort() == 0);

    std::cout << "Phase 6 video client test passed\n";
}
