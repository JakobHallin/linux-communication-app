#include <iostream>
#include "http_tcpServer.h"

int main() {
    std::cout << "starting program" << std::endl;
    http::TcpServer server(8080, true);
    int result = server.start();

    if (result != 0) {
        std::cerr << "Failed to start TCP server\n";
        return 1;
    }
    return 0;
}
