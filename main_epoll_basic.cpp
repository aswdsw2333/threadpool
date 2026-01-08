#include<iostream>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string>
#include<arpa/inet.h>
#include<vector>
#include<cstring>
#include<fcntl.h>
using namespace std;

#define MAX_EVENTS 10
#define PORT 5005
#define BUFFER_SIZE 1024

// 设置 socket 为非阻塞模式 (关键！)
void setnonblocking(int sockfd) {
    // 暂时略过具体实现，epoll 高效通常配合非阻塞 IO
    // 在简单的 demo 中，即使阻塞模式 epoll 也能工作，但推荐非阻塞
}

int main()
{


    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_add;
    server_add.sin_family = AF_INET;
    server_add.sin_port = htons(PORT);
    server_add.sin_addr.s_addr = INADDR_ANY;//设置任意端口可以过来进行通讯

    int opt = 1;//设置端口复用
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

	bind(listen_fd, (sockaddr*)&server_add, sizeof(server_add));//绑定端口
	listen(listen_fd, 5);//开始监听，设置最大监听数为 5

    //2. 创建 Epoll 实例 (监控中心)
	int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
		cout << "epoll_create1 failed" << endl;
        return -1;
    }
	//3. 注册监听 socket 到 Epoll 实例
    epoll_event ev{};
	ev.events = EPOLLIN; // 监听可读事件/,我们关心：是否有新连接进来 (EPOLLIN)
	ev.data.fd = listen_fd;//监听 socket 的 fd 记得记录是谁
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1)//这段代码的作用是把监听
        //套接字注册到 epoll 实例里，以便通过 epoll_wait 被通知“有新连接到来”或其它事件。
    {
		perror("epoll_ctl: listen_sock");
        return 1;
    }
	vector<epoll_event> epoll_events(MAX_EVENTS);//用于存放 epoll_wait 返回的事件列表
	cout << "Server started on port" << PORT << endl;
	//4. 事件循环
    while (true)
    {
        // 等待事件发生。-1 表示永久等待，直到有事发生。
        // 返回值 nfds 是发生事件的个数。
        int nfds = epoll_wait(epoll_fd, epoll_events.data(), MAX_EVENTS, -1);
        if (nfds == -1)
        {
			perror("epoll_wait");
            break;
        }
        for (int i = 0; i < nfds; ++i)//处理每个发生的事件
        {
			int current_fd = epoll_events[i].data.fd;
            if (current_fd == listen_fd)//监听socket有新连接到来
            {
                sockaddr_in client_addr{};
				socklen_t client_addr_len = sizeof(client_addr);
                int client_fd = accept(current_fd, (sockaddr*)&client_addr, &client_addr_len);
                if (client_fd != -1)
                {
					cout << "New connection accepted, fd: " << client_fd << endl;
                    // 【关键】：把新来的客户端也加入 epoll 监控
                    ev.events = EPOLLIN; // 关心它发没发数据
                    ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                }
            }
			else {//已有连接发来数据
                char buffer[BUFFER_SIZE] = {0};
				ssize_t n = recv(current_fd, buffer, BUFFER_SIZE, 0);
                if (n > 0)
                {
                    cout << "Received data from fd " << current_fd << ": " << string(buffer, n) << endl;
					string msg = "Echo: " + string(buffer, n);
                    send(current_fd, msg.c_str(), msg.size(), 0);//回显数据
                }
                else if (n == 0)
				{
                    //客户端关闭连接
                     cout << "Client disconnected, fd: " << current_fd << endl;
                     // 【关键】：从 epoll 监控中移除
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, nullptr);
                    close(current_fd);
                }
                else
				{
                    perror("recv");
                    close(current_fd);
                }

            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;

}