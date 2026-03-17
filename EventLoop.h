#pragma once
#include <memory>
#include <unordered_set>
#include <vector>
#include<functional>
#include<thread>
#include<mutex>
#include <sys/eventfd.h> // 必须引入！
#include <unistd.h>      // read/write
#include<sys/timerfd.h>
#include <algorithm>     // for std::swap
#include<iostream>
#include "TcpConnection.h"
class TcpConnection; // 前置声明
using TcpConnectionPtr = std::shared_ptr<TcpConnection>; // 假设你用 shared_ptr 管理连接
class Epoll;   // 前置声明
class Channel; // 前置声明


class EventLoop {
public:
	using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    void updateChannel(Channel* channel);

    // 在当前 Loop 线程执行任务
    // 如果是当前线程 -> 立即执行
    // 如果是其他线程 -> 排队 (queueInLoop)
    void runInLoop(Functor cb);

    // 把任务放入队列，并唤醒 Loop
    void queueInLoop(Functor cb);

    // 唤醒 Loop (向 eventfd 写数据)
    void wakeup();

    // 判断当前线程是否是 Loop 所在的线程
    bool isInLoopThread() const { return threadId_ == std::this_thread::get_id(); }
    void addConnection(const TcpConnectionPtr& conn);

    void removeConnection(const TcpConnectionPtr& conn);
private:
    // 处理唤醒事件 (读取 eventfd)
    void handleRead();

    // 执行队列里所有的任务
    void doPendingFunctors();
private:
    bool looping_;
    // 【新增生死簿】：记录分配给这个 Loop 的所有连接
    std::unordered_set<TcpConnectionPtr> connections_;
    bool quit_;
    std::unique_ptr<Epoll> epoll_; // Loop 独占 Epoll
	std::vector<Functor> pendingFunctors_;// 存储需要执行的回调函数,也就是任务队列
	int wakeupFd_; // 用于唤醒 Loop 的文件描述符
    int timerFd_;//用于每一个事件循环的定时器
    std::unique_ptr<Channel> timerChannel_; // 【新增】定时器的专属 Channel (用原始指针也可以，记得析构)
    // 【新增】处理定时器事件的回调函数
    void handleTimerRead();
	std::mutex mutex_; // 保护 pendingFunctors_
    const std::thread::id threadId_;//防止死锁,判断我是不是在自己的线程；
	std::unique_ptr<Channel> wakeupChannel_; // 用于监听 wakeupFd_ 的 Channel
    // 标志：当前是否正在执行 pendingFunctors
    // (为了防止在执行任务时，任务又想添加新任务导致的死锁或逻辑错误)
    bool callingPendingFunctors_;// 标志：当前是否正在执行 pendingFunctors
    // (为了防止在执行任务时，任务又想添加新任务导致的死锁或逻辑错误)
};

