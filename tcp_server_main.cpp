#include "tcp_server.h"
#include <iostream>

int main() {
    try {
        TcpServer server(8000);
        server.start();
    } catch(std::exception& e) {
        std::cout << "错误" << e.what() << std::endl;
    }
}