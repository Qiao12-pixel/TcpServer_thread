#pragma once
#include "tcp_client.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <string>

class TcpClient {
public:
    TcpClient(const std::string& ip, int port);
    ~TcpClient();
    void connect_server();
    void send_msg(const std::string& msg);
    std::string recv_msg();
    bool is_connected() const {
        return is_connected_;
    }



private:
    int sock_fd_;
    std::string server_ip_;
    int server_port_;
    bool is_connected_;
};