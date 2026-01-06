#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h> // 【核心】Epoll 头文件

#define PORT 8000
#define MAX_EVENTS 1024 // 每次 epoll_wait 最多返回多少个就绪事件

int main() {
    // -----------------------------------------------------------
    // 1. 基础搭建：Socket -> Bind -> Listen
    // -----------------------------------------------------------
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return -1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    //端口复用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        return -1;
    }

    std::cout << "[Epoll Server] 启动，监听端口: " << PORT << std::endl;

    // -----------------------------------------------------------
    // 2. 创建 Epoll 实例 (epoll_create)
    // -----------------------------------------------------------
    // 参数 0 在现代 Linux 中被忽略，只要大于 0 即可，或者用 epoll_create1(0)
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        return -1;
    }

    // -----------------------------------------------------------
    // 3. 把 Server Socket 添加到 Epoll 监控名单 (epoll_ctl)
    // -----------------------------------------------------------
    struct epoll_event event;
    event.events = EPOLLIN; // 关心 "可读" 事件 (有新连接也算可读)
    event.data.fd = server_fd; // 当事件发生时，告诉我这个 fd 是 server_fd

    // 动作：EPOLL_CTL_ADD (添加)
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl: server_fd");
        return -1;
    }

    // 准备一个数组，用来接收 epoll_wait 返回的就绪事件
    // 只要有事件发生，内核就会把数据填到这里面
    struct epoll_event events[MAX_EVENTS];

    // -----------------------------------------------------------
    // 4. 事件循环 (Event Loop)
    // -----------------------------------------------------------
    while (true) {
        // 等待事件发生 (阻塞)
        // -1 表示永久等待，直到有事件
        // 返回值 nfds 是就绪事件的个数
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        if (nfds == -1) {
            perror("epoll_wait failed");
            break;
        }

        // 循环处理每一个就绪的事件
        // 【注意】这里只需要遍历 nfds 次;不需要遍历所有连接！
        for (int i = 0; i < nfds; ++i) {
            int current_fd = events[i].data.fd;

            // 情况 A: 监听 Socket 就绪 -> 说明有新连接
            if (current_fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                
                if (client_fd == -1) {
                    perror("accept failed");
                    continue;
                }

                std::cout << "新连接: " << client_fd << " IP: " << inet_ntoa(client_addr.sin_addr) << std::endl;

                // 【关键】把新来的 client_fd 也加到 Epoll 监控名单里
                event.events = EPOLLIN; // 默认是水平触发 (Level Triggered)
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll_ctl: client_fd");
                    close(client_fd);
                }
            }
            // 情况 B: 客户端 Socket 就绪 -> 说明有数据发来了
            else {
                char buffer[1024] = {0};
                int valread = read(current_fd, buffer, 1024);

                if (valread <= 0) {
                    // 读到 0 表示客户端断开，或者出错
                    std::cout << "客户端断开: " << current_fd << std::endl;
                    
                    // 【关键】从 Epoll 名单中删除
                    // close 会自动将 fd 从 epoll 中移除，但显式调用 epoll_ctl 删除是好习惯
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    close(current_fd);
                } else {
                    std::cout << "[Recv " << current_fd << "]: " << buffer << std::endl;
                    
                    // 回显 (Echo)
                    std::string reply = "Epoll Server: " + std::string(buffer);
                    send(current_fd, reply.c_str(), reply.length(), 0);
                }
            }
        }
    }
    close(server_fd);
    close(epoll_fd);
    return 0;
}