#pragma once
#include <string>
#include <netinet/in.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <thread>
#include <mutex>

class TcpServer {
public:
    explicit TcpServer(int port);
    ~TcpServer();
    void start();

private:
    int server_fd_;
    int port_;
    bool is_running_;
    void hand_client(int client_fd);//处理单个客户端的通信逻辑
};