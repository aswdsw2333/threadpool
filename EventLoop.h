
#pragma once
#include <memory>
#include <vector>

class Epoll;   // 前置声明
class Channel; // 前置声明

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    void updateChannel(Channel* channel);

private:
    bool looping_;
    bool quit_;
    std::unique_ptr<Epoll> epoll_; // Loop 独占 Epoll
};

