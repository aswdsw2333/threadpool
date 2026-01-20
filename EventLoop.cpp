
// EventLoop.cpp
#include "EventLoop.h"
#include "epoll.h"
#include "Channel.h"

EventLoop::EventLoop()
    : looping_(false), quit_(false), epoll_(new Epoll()) // 创建 Epoll 对象
{
}

EventLoop::~EventLoop() {}

void EventLoop::updateChannel(Channel* channel) {
    epoll_->updateChannel(channel);
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        std::vector<Channel*> channels = epoll_->poll();
        for (auto* channel : channels) {
            channel->handleEvent();
        }
    }
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
}