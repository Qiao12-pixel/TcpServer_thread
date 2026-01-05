#include "tcp_client.h"

TcpClient::TcpClient(const std::string& ip, int port) :
    server_ip_(ip), server_port_(port), sock_fd_(-1), is_connected_(false) {
    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd_ < 0) {
        throw std::runtime_error("socket创建失败");
    }
}
TcpClient::~TcpClient() {
    if (sock_fd_ == 0) {
        close(sock_fd_);
        std::cout << "[client]资源已释放" << std::endl;
    }
}
void TcpClient::connect_server() {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port_);
    //IP地址转换
    if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0) {
        throw std::runtime_error("无效的IP地址");
    } if (connect(sock_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("连接服务器失败");
    }
    is_connected_ = true;
    std::cout << "[client]已连接到" << server_ip_ << ":" << server_port_ << std::endl;
}

void TcpClient::send_msg(const std::string& msg) {
    if (!is_connected_) {
        return;
    }
    char buffer[1024] = {0};
    send(sock_fd_, msg.c_str(), msg.length(), 0);
}
std::string TcpClient::recv_msg() {
    if(!is_connected_) {
        return "";
    }
    char buffer[1024] = {0};
    int val_read = read(sock_fd_, buffer, 1024);
    if (val_read > 0) {
        return std::string(buffer);
    }
    return "";
}