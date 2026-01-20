#include<iostream>

#include"Channel.h"
#include"epoll.h"
#include"EventLoop.h"
#include"EventLoopThreadPool.h"
#include "Tcpserver_epoll.h"

using namespace std;

int main()
{
        Tcpserver_epoll server(5005);

        // 设置 4 个子线程
        // 这样你有：1个主线程(accept) + 4个工子线程(read/write)
        server.setThreadNum(4);

        server.start();
        return 0;

}