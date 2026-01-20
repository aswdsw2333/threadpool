#include "Tcpserver_epoll.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
// 全局变量加锁保护
using namespace std;
// ✅ 改成 extern 声明
map<int, std::shared_ptr<Channel>> connectionMap;
mutex coutMutex;
map<int, Channel*> channel_map;
std::mutex g_connMutex; // [新增] 用于保护 connectionMap
std::mutex g_coutMutex; // [新增] 用于保护 cout
Tcpserver_epoll::Tcpserver_epoll(int port):listen_fd(-1),port(PORT), max_connect_(MAX_CONNECTIONS), max_threadnums(MAX_THREAD_NUMS), threadPool_(new EventLoopThreadPool(&loop))
{
    memset(&server_add, 0, sizeof(server_add));
    server_add.sin_family = AF_INET;
    server_add.sin_port = htons(port);
    server_add.sin_addr.s_addr = INADDR_ANY;//设置任意端口可以过来进行通讯
}
Tcpserver_epoll::~Tcpserver_epoll() 
{
    if (listen_fd != -1)
    {
        close(listen_fd);
        return;
   }
}

void Tcpserver_epoll::start()
{
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket error"); // 打印错误原因
        return;
    }
    
    int opt = 1;//设置端口复用
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    
    if (bind(listen_fd, (sockaddr*)&server_add, sizeof(server_add)) < 0) {
        perror("bind error"); // 【关键】：如果端口被占，这里会报错 "Address already in use"
        return;
    }
    listen(listen_fd, max_connect_);//开始监听，设置最大监听数为 20
    if (listen(listen_fd, max_connect_) < 0) {
        perror("listen error");
        return;
    }
    // 必须把监听 socket 设为非阻塞（你在 Channel::enableReading 使用了 EPOLLET）
    setnonblocking(listen_fd);

    cout << "server listening on port 5005" << endl;
    // [修改 2] 启动线程池 (如果不调用 setThreadNum，默认还是单线程)
    threadPool_->start();
    cout << "ThreadPool started." << endl; // <--- 加这一行
    // ServerChannel 归主线程 (&loop) 管
    auto Serverchannel = make_shared<Channel>(&loop, listen_fd);
    // 这里的引用捕获 [&] 要小心，但在 start() 阻塞在 loop.loop() 期间是安全的
    Serverchannel->setReadCallback([&, Serverchannel]() {
        while (true) {
            struct sockaddr_in Client_addr {};
            socklen_t Client_len = sizeof(Client_addr);
            int Client_fd = accept(listen_fd, (sockaddr*)&Client_addr, &Client_len);

            if (Client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                else { perror("accept error"); break; }
            }

            {
                std::lock_guard<std::mutex> lock(g_coutMutex);
                cout << "new client connection, fd=" << Client_fd << " [Main Thread]" << endl;
            }

            setnonblocking(Client_fd);

            // [修改 3 - 核心] 从线程池拿一个 SubLoop
            EventLoop* ioLoop = threadPool_->getNextLoop();

            // [修改 4] 创建 Channel 时，交给 ioLoop，而不是主 loop
            auto ClientChannel = make_shared<Channel>(ioLoop, Client_fd);

            ClientChannel->setReadCallback([Client_fd, ClientChannel]() {
                while (true) {
                    char buf[1024] = { 0 };
                    ssize_t n = read(Client_fd, buf, sizeof(buf));
                    if (n > 0) {
                        {
                            std::lock_guard<std::mutex> lock(g_coutMutex);
                            // 打印当前线程ID，验证是否在子线程
                            std::cout << "Thread[" << std::this_thread::get_id() << "] Recv: " << std::string(buf, n) << std::endl;
                        }
                        string msg2 = string(buf, n) + "nihao";
                        write(Client_fd, msg2.c_str(), msg2.size());
                    }
                    else if (n == 0) {
                        {
                            std::lock_guard<std::mutex> lock(g_coutMutex);
                            std::cout << "Client " << Client_fd << " disconnected." << std::endl;
                        }
                        ClientChannel->disableAll();
                        close(Client_fd);

                        // [修改 5] 加锁操作全局 Map
                        {
                            std::lock_guard<std::mutex> lock(g_connMutex);
                            connectionMap.erase(Client_fd);
                        }
                        break;
                    }
                    else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        ClientChannel->disableAll();
                        close(Client_fd);
                        {
                            std::lock_guard<std::mutex> lock(g_connMutex);
                            connectionMap.erase(Client_fd);
                        }
                        break;
                    }
                }
                });

            ClientChannel->enableReading();

            // [修改 6] 加锁保存连接
            {
                std::lock_guard<std::mutex> lock(g_connMutex);
                connectionMap[Client_fd] = ClientChannel;
            }
        }
        });
                // 4. 开启读监听 (告诉 epoll 开始干活)
                Serverchannel->enableReading();
                // 把 ServerChannel 存起来防止析构，这里演示暂时存 map 里，实际建议 Server 类持有
                {
                    std::lock_guard<std::mutex> lock(g_connMutex);
                    connectionMap[listen_fd] = Serverchannel;
                }

                loop.loop(); // 主线程死循环
}



void Tcpserver_epoll::setnonblocking(int sockfd) {
    int opts = fcntl(sockfd, F_GETFL);
    if (opts < 0)
    {
        perror("fcntl(F_GETFL) failed");
        return;
    }
    opts = opts | O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, opts) < 0)
    {
        perror("fcntl(F_SETFL) failed");
        return;
    }
    return;
}
