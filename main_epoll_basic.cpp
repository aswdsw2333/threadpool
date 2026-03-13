#include<iostream>
#include <google/protobuf/message.h>
#include "rpc.pb.h"
#include"Channel.h"
#include"epoll.h"
#include"EventLoop.h"
#include"EventLoopThreadPool.h"
#include "Tcpserver_epoll.h"

#include "RpcProvider.h"
#include "UserServiceImpl.h" // 包含你的业务类

int main() {
    // 1. 实例化 RPC 框架对象
    RpcProvider provider;

    // 2. 将你的服务业务挂载到框架上 (就像即插即用的 U 盘一样！)
    provider.NotifyService(new UserServiceImpl());

    // 3. 启动服务器 (阻塞在这里运行 Epoll)
     provider.Run(5005,4); 

    return 0;
}