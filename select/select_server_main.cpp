#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h> // 【核心头文件】

#define PORT 8000
#define MAX_CLIENTS 10  // select 默认限制通常是 1024，这里演示只用 10

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 3);
    
    std::cout << "[Select Server] 启动，监听端口: " << PORT << std::endl;

    // 2. 准备客户端列表
    int client_sockets[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) client_sockets[i] = 0; // 0 表示空闲

    // 3. 核心循环 (Event Loop)
    while (true) {
        // --- 步骤 A: 准备文件描述符集合 (Set) ---
        fd_set readfds;      // 定义一个集合
        FD_ZERO(&readfds);   // 清空集合

        // 把 "ServerSocket" 加入集合 (为了接新电话)
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        // 把所有已连接的 "ClientSocket" 加入集合 (为了收消息)
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds); // 如果有效，就加入监控
            if (sd > max_sd) max_sd = sd;     // 更新最大的 fd 值 (select 需要)
        }

        // --- 步骤 B: 阻塞等待 (Wait) ---
        // 这一步是阻塞的！只要集合里有任何一个 socket 有动静，就会返回
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            std::cout << "Select error" << std::endl;
        }

        // --- 步骤 C: 醒来后，挨个检查 (Dispatch) ---
        
        // 1. 检查是不是 ServerSocket 动了 (说明有新连接)
        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket = accept(server_fd, (struct sockaddr*)&address, &addr_len);
            std::cout << "新连接到来, FD: " << new_socket << std::endl;

            // 找个空位存起来
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    std::cout << "添加到列表, 索引: " << i << std::endl;
                    break;
                }
            }
        }

        // 2. 检查是不是某个 ClientSocket 动了 (说明有消息)
        // 【痛点】：这里必须遍历所有客户端，效率低就在这！
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            
            if (FD_ISSET(sd, &readfds)) { // 如果这个客户端在就绪集合里
                char buffer[1024] = {0};
                int valread = read(sd, buffer, 1024);

                if (valread == 0) {
                    // 客户端断开
                    std::cout << "客户端断开, FD: " << sd << std::endl;
                    close(sd);
                    client_sockets[i] = 0; // 从列表移除
                } else {
                    // 收到消息，回显
                    buffer[valread] = '\0';
                    std::cout << "收到消息: " << buffer << std::endl;
                    send(sd, buffer, strlen(buffer), 0);
                }
            }
        }
    }
    return 0;
}