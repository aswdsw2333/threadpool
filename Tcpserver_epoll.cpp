#include "Tcpserver_epoll.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include"TcpConnection.h"
#include <memory> // 在这里添加头文件
// 全局变量加锁保护
using namespace std;

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
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    
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
    // 修复陷阱2：只捕获 this，避免循环引用
    Serverchannel->setReadCallback([this]() {
        std::cout << "\n=> [大堂经理] 警报！门口有客人来了！开始批量接待..." << std::endl;
        int accept_count = 0; // 记录本次到底接待了几个

        while (true)
        {
            // 🚀 修复陷阱1：必须把长度重置放在循环内部！每次 accept 前都是干净的！
            struct sockaddr_in client_addr;
            socklen_t Client_sock = sizeof(client_addr);

            int Client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &Client_sock);

            if (Client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::cout << "=> [大堂经理] 队列已彻底榨干！本次共接待了 " << accept_count << " 个客人，回去睡觉。\n" << std::endl;
                    break;
                }
                else if (errno == EINTR) {
                    // 被系统信号打断，这不是错误，必须继续！
                    continue;
                }
                else {
                    // 打印极其详细的错误，看看到底是谁阻碍了接待
                    perror("=> [大堂经理] accept 发生致命错误");
                    break;
                }
            }

            accept_count++;
            std::cout << "  -> 成功接待第 " << accept_count << " 个连接，分配 FD: " << Client_fd << std::endl;

            // 调用你的业务逻辑，丢给子线程
            newConnection(Client_fd);
        }
        });
    // 4. 开启读监听 (告诉 epoll 开始干活)
    Serverchannel->enableReading();
    // 依然需要保存 serverChannel 否则会析构，但现在最好放在 Server 类成员里
        // 这里为了演示简单，可以先暂时不做处理，或者加一个 vector<shared_ptr<void>> keepAlive_
        // 在真实项目中，Acceptor 会被封装成一个类。
        // *为了防止 serverChannel 析构，我们不仅要把它注册进 epoll，还得持有它*
        // 临时方案：用一个成员变量持有它，或者为了编译通过，我们可以用一个静态变量（虽丑但管用）
    static auto keepAlive = Serverchannel;

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

void Tcpserver_epoll::newConnection(int sockfd)
{
    // 1. 选择一个子 EventLoop
    EventLoop* ioLoop = threadPool_->getNextLoop();
    // 2. 创建 TcpConnection 对象
	string connName = "Conn-" + to_string(nextConnId_++);
	cout << "new connection" << connName <<"fd" <<sockfd<< endl;
    TcpConnectionPtr conn = make_shared<TcpConnection>(ioLoop, connName, sockfd);
    // 4. 将连接存入 map (Server 持有连接的生命周期)
    connections_[connName] = conn;

    // 5. 设置回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);

    // 【关键】设置关闭回调
    // 绑定 this->removeConnection，当连接断开时，TcpConnection 会调这个函数
    conn->setCloseCallback(
        std::bind(&Tcpserver_epoll::removeConnection, this, std::placeholders::_1)
    );
    // 5. 【登记二】：通知分公司，把这个连接写进它的“本地小本子”！
    // 极其关键：我们不能直接调用 ioLoop->addConnection(conn)！
    // 因为这依然是在主线程里修改子线程的变量！
    // 我们必须用 runInLoop 的魔法，写一张“便利贴”，让子线程自己去把它记在小本子上！
    ioLoop->runInLoop(std::bind(&EventLoop::addConnection, ioLoop, conn));

    // 6. 关键一步！让 ioLoop 里的 TcpConnection 开始工作
    //    不能直接调用 connectEstablished，因为那是跨线程调用 epoll_ctl
    //    必须用 runInLoop 切到子线程去执行
    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn)
    );
   
}
void Tcpserver_epoll::removeConnection(const TcpConnectionPtr& conn)
{
    // 这里是主线程调用的
    // 为了线程安全，把删除操作放到对应的 SubLoop 里执行
    loop.runInLoop(
        std::bind(&Tcpserver_epoll::removeConnectionInLoop, this, conn)
    );
}
// 【核心逻辑】移除连接 (Phase 2)
// 这个函数在主线程运行，安全地删除 map 里的元素
void Tcpserver_epoll::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    cout << "Removing connection " << conn->name() << endl;

    // 1. 从 map 中删除，引用计数 -1
    size_t n = connections_.erase(conn->name());
    (void)n; // 消除未使用变量警告

    // 2. 此时 conn 的引用计数还没归零（因为参数里还传了一份）
    //    我们需要让 connection 在它自己的 Loop 里做最后的清理（stop epoll）
    EventLoop* ioLoop = conn->getLoop();

    // 3. 切回子线程做最后销毁
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn)
    );
}
