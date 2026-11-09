#include "screen_video_client.h"

#include <cassert>
#include <iostream>

int main() {
    client::ScreenVideoClient client("127.0.0.1");
    assert(client.start());
    assert(client.localPort() != 0);
    client.setMediaPort(40000);
    client.setIdentity(1, 2);
    client.stop();
    assert(client.localPort() == 0);

    std::cout << "Phase 7 screen video client test passed\n";
}
