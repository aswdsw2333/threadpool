
#include "Eventloopthread.h"
EventLoopThread::EventLoopThread()
    : loop_(nullptr),
    exiting_(false),
    thread_() // 构造时线程还没启动
{
}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_) {
        loop_->quit(); // 退出循环
        thread_.join(); // 等待线程结束
    }
}

EventLoop* EventLoopThread::startLoop() {
    // 启动线程，执行 threadFunc
    thread_ = std::thread(&EventLoopThread::threadFunc, this);

    // 【关键同步】：必须等待线程里的 loop_ 初始化完成才能返回
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) {
            cond_.wait(lock);
        }
    }
    return loop_;
}

void EventLoopThread::threadFunc() {
    EventLoop loop; // 【重点】：Loop 是栈上对象，生命周期与线程一致

    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one(); // 通知主线程：Loop 创建好了，你可以返回了
    }

    loop.loop(); // 开始死循环

    // 循环退出后
    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
