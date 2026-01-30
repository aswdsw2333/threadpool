
#pragma once
#include <memory>
#include <functional>

class Buffer;
class TcpConnection;
class Timestamp; // 暂时还没写，可以先不管，或者用 string 代替

// 使用 shared_ptr 管理连接的生命周期
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

// 连接建立/断开的回调
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;

// 消息到达的回调 (用户最关心的)
// 参数：连接指针、接收缓冲区、接收时间
using MessageCallback = std::function<void(const TcpConnectionPtr&,
    Buffer*,
    long long)>; // 暂时用 long long 代表时间戳

// 数据写完的回调 (高水位回调，用于流量控制，暂时备用)
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
// 连接关闭的回调
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
