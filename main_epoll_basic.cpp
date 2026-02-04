#include<iostream>

#include"Channel.h"
#include"epoll.h"
#include"EventLoop.h"
#include"EventLoopThreadPool.h"
#include "Tcpserver_epoll.h"

// 用户逻辑：当收到消息时
// main.cpp
void onMessage(const TcpConnectionPtr& conn, Buffer* buf, long long time) {
    // 循环：解决粘包
    while (buf->readableBytes() >= 4) {

        // 1. 偷看长度 (不移动指针！)
        int32_t len = buf->peekInt32();

        // 2. 校验长度 (防止恶意攻击)
        if (len > 65536 || len < 0) {
            // 这种包直接踢掉，不然会撑爆内存
            conn->shutdown();
            break;
        }

        // 3. 判断是否收到了完整的包
        // 现在的 readableBytes = Header(4) + Body(Buffer里的剩余)
        if (buf->readableBytes() >= 4 + len) {

            // 4. 终于可以读走了
            buf->retrieve(4); // 移走头部

            // 5. 取出正文
            string msg = buf->retrieveAsString(len); // 移走 len 长度的数据

            // 6. 业务回调
            // handleMessage(msg);
            std::cout << "Got Message: " << msg << std::endl;
        }
        else {
            // 数据不够，说明分包了，break 出去等下一次数据来
            break;
        }
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