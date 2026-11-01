#include <iostream>
#include "http_tcpServer.h"

int main() {
    std::cout << "starting program" << std::endl;
    http::TcpServer server;
    int result = server.start();

    if (result != 0) {
        std::cerr << "Failed to start TCP server\n";
        return 1;
    }
    return 0;
}
