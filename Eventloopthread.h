#pragma once
#include "EventLoop.h"
#include <thread>
#include <mutex>
#include <condition_variable>

class EventLoopThread {
public:
    EventLoopThread();
    ~EventLoopThread();

    EventLoop* startLoop(); // 启动线程，并返回那个线程里的 Loop 指针，只有条件变量允许才启动

private:
    void threadFunc();      // 线程函数//

    EventLoop* loop_;       // 也就是这个线程运行的 Loop
    bool exiting_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

