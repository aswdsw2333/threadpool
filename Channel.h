// Channel.h
#pragma once
#include <sys/epoll.h>
#include <functional>

class EventLoop; // 前置声明，防止头文件循环引用

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent();
    void enableReading();

    int fd() const { return fd_; }
    uint32_t events() const { return events_; }
    void set_revents(uint32_t revt) { revents_ = revt; }

    // 状态机索引：-1: kNew, 1: kAdded, 2: kDeleted
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }
    void disableAll(); // 新增：取消所有事件
    void remove();     // 新增：从 Loop 中移除自己
    void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb); }

private:
    void update();

    EventLoop* loop_; // 核心：持有 Loop 指针
    const int fd_;
    uint32_t events_;
    uint32_t revents_;
    int index_;
    EventCallback readCallback_;
};

