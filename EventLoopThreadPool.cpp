
#include "EventLoopThreadPool.h"
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop)
    : baseLoop_(baseLoop),
    started_(false),
    numThreads_(0),
    next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool() {
    // 不需要手动释放 Loop，因为它们是在 EventLoopThread 的栈上
}

void EventLoopThreadPool::start() {
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        std::unique_ptr<EventLoopThread> t(new EventLoopThread());
        loops_.push_back(t->startLoop()); // 启动线程并保存 Loop 指针
        threads_.push_back(std::move(t));
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    EventLoop* loop = baseLoop_;

    // 如果设置了多线程，就轮询选择
    if (!loops_.empty()) {
        loop = loops_[next_];
        next_ = (next_ + 1) % loops_.size();
    }
    return loop;
}
