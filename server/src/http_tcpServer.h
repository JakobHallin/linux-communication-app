#ifndef INCLUDE_HTTP_TCP_SERVER_H
#define INCLUDE_HTTP_TCP_SERVER_H

namespace http {
    class TcpServer {
    public:
        TcpServer();
        ~TcpServer();
        int start();
        void closeServer();
    private: 
    int serverSocket = -1;
    };
} // namespace http

#endif // INCLUDE_HTTP_TCP_SERVER_H