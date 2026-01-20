#pragma once
#include "EventLoop.h"
#include "Eventloopthread.h"
#include <vector>
#include <memory>
#include<mutex>

class EventLoopThreadPool {
public:
    EventLoopThreadPool(EventLoop* baseLoop);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start();

    // 核心功能：通过轮询算法获取下一个 Loop
    EventLoop* getNextLoop();

private:
    EventLoop* baseLoop_; // 主 Loop (如果不开启多线程，所有任务都给它)
    bool started_;
    int numThreads_;
    int next_; // 轮询索引

    std::vector<std::unique_ptr<EventLoopThread>> threads_; // 管理线程对象
    std::vector<EventLoop*> loops_; // 纯指针列表，方便快速访问
};

