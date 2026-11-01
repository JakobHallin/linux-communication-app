#include "video_client.h"

#include <cassert>
#include <iostream>

int main() {
    client::VideoClient client("127.0.0.1", "/dev/video-does-not-exist");
    assert(client.start());
    assert(client.localPort() != 0);
    client.setMediaPort(40000);
    client.setIdentity(1, 2);
    assert(!client.activate());
    client.stop();
    assert(client.localPort() == 0);

    std::cout << "Phase 6 video client test passed\n";
}
