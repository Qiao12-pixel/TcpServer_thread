#include "tcp_client.h"

int main() {
    try
    {
        TcpClient client("127.0.0.1", 8000);
        client.connect_server();
        std::string input;
        while (true)
        {
            std::cout << "请输入需要传输的数据[输入exit退出]:" << std::endl;
            std:;getline(std::cin, input);
            if (input == "exit") {
                break;
            }
            client.send_msg(input);
            std::string reply = client.recv_msg();
            std::cout << "服务器回复:" << reply << std::endl;
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "服务器端错误: " << e.what() << '\n';
    }
    
}