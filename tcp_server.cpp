#include "tcp_server.h"
std::mutex cout_mutex;
TcpServer::TcpServer(int port)
    : port_(port), server_fd_(-1), is_running_(false) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("socket创建失败");
    }
    //设置端口复用 (避免重启时 bind error)
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("bind失败");
    } if (listen(server_fd_, 50) < 0) {
        close(server_fd_);
        throw std::runtime_error("listen失败");
    }
     std::cout << "[Server]服务端初始化成功,端口:" << port << std::endl;
}
TcpServer::~TcpServer() {
    if (server_fd_ == 0) {
        close(server_fd_);
        std::cout << "[Server]资源已释放" << std::endl;
    }
}

void TcpServer::start() {
    is_running_ = true;
    std::cout << "[Server] 等待客户连接 " << std::endl;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while (is_running_)
    {
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0 ) {
            std::cerr << "[Error]接受连接失败" << std::endl;
            continue;
        }
        {
            std::unique_lock<std::mutex> lock(cout_mutex);
            std::cout << "[Server]新客户端连接" << inet_ntoa(client_addr.sin_addr)//转本地字节序
                    << "(FD:" << client_fd << ")" << std::endl;            
        }
        //创建新线程处理该客户端
        std::thread client_thread(&TcpServer::hand_client, this, client_fd);
        client_thread.detach();
    }
}
void TcpServer::hand_client(int client_fd) {
    char buffer[1024] = {0};
    while (true)
    {
        memset(buffer, 0, sizeof(buffer));//调用内存置零函数，将buffer的所有字节（共 1024 字节）设置为 0。
        int val_read = read(client_fd, buffer, 1024);
        if (val_read > 0 ) {
            {
                std::unique_lock<std::mutex> lock(cout_mutex);
                std::cout << "[Server]收到消息:" << buffer << std::endl;
            }   
            std::string reply = "服务端已经收到: " + std::string(buffer);
            send(client_fd, reply.c_str(),reply.length(), 0);
            
        } else if (val_read == 0) {
            std::cout << "[Server] 客户端断开连接" << std::endl;
            break;
        } else {
            std::cerr << "[Error] 读取出错" << std::endl;
            break;
        }
    }
    close(client_fd);
    std::cout << "关闭客户端连接" << std::endl;
}