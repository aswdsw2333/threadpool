#include<iostream>
#include "src/game.pb.h" // 👈 引入生成的头文件
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


// 假设这是你的连接回调
void onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        std::cout << "New connection! Sending LoginResponse..." << std::endl;

        // 1. 创建 Protobuf 对象并赋值
        GameMsg::LoginResponse resp;
        resp.set_success(true);
        resp.set_msg("Welcome to the 211 Radar Server!");
        resp.set_error_code(0);

        // 2. 序列化 (Protobuf -> string)
        std::string binaryData;
        if (!resp.SerializeToString(&binaryData)) {
            std::cerr << "Serialization failed!" << std::endl;
            return;
        }

        // 3. 封包 (Length + Body)
        Buffer sendBuf;

        // 3.1 先写包头 (长度)
        // 注意：这里写入的是 binaryData 的长度，不包含包头本身的4字节
        sendBuf.appendInt32(static_cast<int32_t>(binaryData.size()));

        // 3.2 再写包体 (Protobuf 数据)
        sendBuf.append(binaryData);

        // 4. 发送
        // 修正：将 Buffer* 转为 string 发送
        conn->send(sendBuf.retrieveAllAsString());
        // 注意：你需要确保 TcpConnection::send 支持 std::string 参数
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