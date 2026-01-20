// Channel.cpp
#include "Channel.h"
#include "EventLoop.h" // 实现文件中包含 EventLoop 头文件

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1) // -1 代表 kNew
{
}

Channel::~Channel() {}

void Channel::update() {
    // Channel -> EventLoop -> Epoll
    loop_->updateChannel(this);
}

void Channel::enableReading() {
    events_ |= EPOLLIN | EPOLLET;
    update();
}

void Channel::handleEvent() {
    if (revents_ & EPOLLIN) {
        if (readCallback_) readCallback_();
    }
}

void Channel::disableAll()
{
    events_ = 0;
    update();    // 同步给 Epoll (即执行 EPOLL_CTL_MOD 把 events 改为 0)
}

void Channel::remove() {
    // 实际项目中这里应该调用 loop_->removeChannel(this);
    // 这里为了演示简单，我们只要确保 disableAll 被调用，epoll 就不会再通知它了
    disableAll();
}




