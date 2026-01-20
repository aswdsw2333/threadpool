// Epoll.cpp
#include "epoll.h"
#include "Channel.h"
#include <iostream>

// 状态常量
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;

Epoll::Epoll() : epollFd_(epoll_create1(EPOLL_CLOEXEC)), events_(1024) {
    if (epollFd_ < 0) {
        perror("epoll_create1 error");
    }
}

Epoll::~Epoll() {
    if (epollFd_ >= 0) close(epollFd_);
}

void Epoll::updateChannel(Channel* channel) {
    const int index = channel->index();
    int fd = channel->fd();

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = channel->events();
    ev.data.ptr = channel; // 核心：绑定 Channel 指针

    if (index == kNew || index == kDeleted) {
        // 新加入或已删除的 -> ADD
        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            perror("epoll_ctl add error");
        }
        channel->set_index(kAdded);
    }
    else {
        // 已经在里面的 -> MOD
        if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
            perror("epoll_ctl mod error");
        }
    }
}

std::vector<Channel*> Epoll::poll(int timeoutMs) {
    std::vector<Channel*> activeChannels;
    int numEvents = epoll_wait(epollFd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);

    if (numEvents < 0) {
        perror("epoll_wait error");
    }
    else if (numEvents > 0) {
        for (int i = 0; i < numEvents; ++i) {
            // 从 data.ptr 取回 Channel 指针
            Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
            channel->set_revents(events_[i].events);
            activeChannels.push_back(channel);
        }
        if (numEvents == static_cast<int>(events_.size())) {
            events_.resize(events_.size() * 2);
        }
    }
    return activeChannels;
}

