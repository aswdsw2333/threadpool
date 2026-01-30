#include<iostream>

#include"Channel.h"
#include"epoll.h"
#include"EventLoop.h"
#include"EventLoopThreadPool.h"
#include "Tcpserver_epoll.h"

// 用户逻辑：当收到消息时
// main.cpp
void onMessage(const TcpConnectionPtr& conn, Buffer* buf, long long time) {
    string msg = buf->retrieveAllAsString();

    // 如果客户端发送 "GET-BIG"，服务器就回吐 50MB 数据
    if (msg == "GET-BIG") {
        // 👇【诊断】打印长度和内容，看看有没有隐藏字符
        std::cout << "Server Recv: " << msg.size() << " bytes. Content: [" << msg << "]" << std::endl;

        // 构造一个 50MB 的字符串 (纯内存操作，极快)
        // 注意：这会瞬间占用服务器 50MB 内存，测试完记得重启
        std::string hugeData(50*1024*1024, 'Z');

        std::cout << "Sending 50MB data..." << std::endl;
        conn->send(hugeData);
    }
    else {
        // 普通 Echo
        conn->send(msg);
    }
}

// 用户逻辑：当连接建立或断开时
void onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        cout << "Connection UP: " << conn->name() << endl;
    }
    else {
        cout << "Connection DOWN: " << conn->name() << endl;
    }
}

int main() {
    Tcpserver_epoll server(5005);
    server.setThreadNum(4);

    // 注册用户逻辑
    server.setMessageCallback(onMessage);
    server.setConnectionCallback(onConnection);

    server.start();
    return 0;
}