#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>      // fcntl: 设置非阻塞
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>      // errno, EAGAIN

#define PORT 8000
#define MAX_EVENTS 1024

// 【工具函数】设置文件描述符为非阻塞模式
// 这是 Epoll ET 模式的硬性要求，否则 read 会卡死
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }
}

int main() {
    // -----------------------------------------------------------
    // 1. 基础搭建：Socket -> Bind -> Listen
    // -----------------------------------------------------------
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket 创建失败");
        return -1;
    }

    // 端口复用 (避免重启时 bind failed)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind 失败");
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen 失败");
        return -1;
    }

    std::cout << "[Epoll Server] 启动，监听端口: " << PORT << std::endl;

    // -----------------------------------------------------------
    // 2. 创建 Epoll 实例
    // -----------------------------------------------------------
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 失败");
        return -1;
    }

    // -----------------------------------------------------------
    // 3. 添加监听 Socket 到 Epoll
    // -----------------------------------------------------------
    struct epoll_event event;
    event.data.fd = server_fd;  
    event.events = EPOLLIN; // server_fd 建议用默认的 LT 模式，处理新连接更稳
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl 添加 server_fd 失败");
        return -1;
    }

    // 用于接收就绪事件的数组
    struct epoll_event events[MAX_EVENTS];

    // -----------------------------------------------------------
    // 4. 事件循环 (Event Loop)
    // -----------------------------------------------------------
    while (true) {
        // 阻塞等待事件发生
        // 返回值 nfds 是就绪的文件描述符数量
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        if (nfds == -1) {
            perror("epoll_wait 错误");
            break;
        }

        // 遍历处理每一个就绪事件
        for (int i = 0; i < nfds; ++i) {
            int current_fd = events[i].data.fd;

            // --- 情况 A: 监听 Socket 有动静 -> 说明有新客户端连接 ---
            if (current_fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                
                if (client_fd == -1) {
                    perror("accept 失败");
                    continue;
                }

                std::cout << "新连接: " << inet_ntoa(client_addr.sin_addr) 
                          << " (FD: " << client_fd << ")" << std::endl;

                // 关键步骤 1必须设为非阻塞！
                setNonBlocking(client_fd);

                // 关键步骤 2添加到 Epoll，并开启 ET (边缘触发)
                event.data.fd = client_fd;
                event.events = EPOLLIN | EPOLLET; 
                
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl 添加 client_fd 失败");
                    close(client_fd);
                }
            }
            // --- 情况 B: 客户端 Socket 有动静 -> 说明有数据发来 ---
            else {
                char buffer[1024]; // 缓冲区
                bool close_conn = false;

                // 【关键步骤 3】ET 模式下的循环读取 (必须把缓冲区读空)
                while (true) {
                    memset(buffer, 0, sizeof(buffer));
                    int valread = read(current_fd, buffer, sizeof(buffer) - 1);

                    if (valread > 0) {
                        // 读到数据，简单的回显 (Echo)
                        std::cout << "[Recv FD " << current_fd << "] " << buffer << std::endl; // 【已取消注释】现在会打印了！
                        send(current_fd, buffer, valread, 0);
                    } 
                    else if (valread == 0) {
                        // 客户端主动断开 (FIN)
                        close_conn = true;
                        break;
                    } 
                    else {
                        // valread == -1，检查错误码
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 【核心】EAGAIN 表示缓冲区空了，数据读完了
                            // 跳出循环，等待下一次通知
                            break;
                        } else {
                            // 真的出错了
                            perror("read 异常");
                            close_conn = true;
                            break;
                        }
                    }
                }

                // 如果需要关闭连接
                if (close_conn) {
                    std::cout << "客户端断开 (FD: " << current_fd << ")" << std::endl;
                    // close 会自动将 fd 从 epoll 树中移除，不需要显式调用 epoll_ctl_del
                    close(current_fd);
                }
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}