// Epoll.h
#pragma once
#include <sys/epoll.h>
#include <vector>
#include <unistd.h>
#include <cstring>

class Channel; // 前置声明

class Epoll {
public:
    Epoll();
    ~Epoll();

    void updateChannel(Channel* channel);
    std::vector<Channel*> poll(int timeoutMs = -1);

private:
    int epollFd_;
    std::vector<struct epoll_event> events_; // 接收 epoll_wait 返回的事件
};

