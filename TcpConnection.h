#pragma once
#include <memory>
#include <string>
#include <atomic>
#include<iostream>
#include "EventLoop.h"
#include "Buffer.h"
#include "Channel.h"
#include "Callbacks.h"
#include <fcntl.h>
// 继承 enable_shared_from_this 的原因：
// 当 TcpConnection 正在处理事件（比如 handleRead）时，它需要把自己传递给用户回调（MessageCallback）。
// 如果直接传 this，用户没办法把它变成 shared_ptr 来延长生命周期。
// 使用 shared_from_this() 可以安全地生成一个指向自己的 shared_ptr。
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
        const std::string& nameArg,
        int sockfd);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    bool connected() const { return state_ == kConnected; }

    // 发送数据 (对外接口)
    void send(const std::string& message);

    // 关闭连接 (对外接口)
    void shutdown();

    // 设置回调函数 (由 Server 传进来)
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; } // 【新增】

    // 连接建立完成时调用 (只调用一次)
    void connectEstablished();
    // 连接销毁时调用 (只调用一次)
    void connectDestroyed();

private:
    // 状态枚举
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

    // 内部事件处理函数 (绑定给 Channel)
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const std::string& message);
    void shutdownInLoop();

    void setState(StateE s) { state_ = s; }
    void setNonBlock(int fd);

private:
    EventLoop* loop_;           // 所属的 SubLoop
    const std::string name_;    // 连接名称
    std::atomic<StateE> state_; // 连接状态

    // 核心组件
    std::unique_ptr<Channel> channel_; // 既然是连接，必然持有一个 Channel
    int socketFd_;                     // 既然是连接，必然持有 socket

    // 缓冲区 (真正的家)
    Buffer inputBuffer_;
    Buffer outputBuffer_; // 暂时还没用到，下一关会用

    // 用户回调
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    //关闭回调
	CloseCallback closeCallback_;
    //写完回调
	WriteCompleteCallback writeCompleteCallback_;
};

