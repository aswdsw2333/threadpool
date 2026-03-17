
// EventLoop.cpp
#include "EventLoop.h"
#include "epoll.h"
#include "Channel.h"
int creatEventfd()
{
	int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);// 创建 eventfd
    if (evtfd < 0) {
        perror("Failed to create eventfd");
		abort();        // 直接终止程序
    }
	return evtfd;
}

int createTimerFd() {
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    struct itimerspec howlong;
    howlong.it_value.tv_sec = 5;      // 5秒后第一次跳动
    howlong.it_value.tv_nsec = 0;
    howlong.it_interval.tv_sec = 5;   // 以后每5秒跳一次
    howlong.it_interval.tv_nsec = 0;
    timerfd_settime(timerfd, 0, &howlong, nullptr);
    return timerfd;
}




EventLoop::EventLoop()
    : looping_(false), 
    quit_(false), 
    epoll_(new Epoll()),// 创建 Epoll 对象
	wakeupFd_(creatEventfd()), // 创建 eventfd
    timerFd_(createTimerFd()),//创建事件定时器
	threadId_(std::this_thread::get_id()), // 记录当前线程 ID
	callingPendingFunctors_(false),
	wakeupChannel_(new Channel(this, wakeupFd_)), // 创建用于监听 wakeupFd_ 的 Channel
    timerChannel_(new Channel(this, timerFd_))
{
	wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));// 设置回调：当有人按铃时，执行 handleRead 把数据读走
	wakeupChannel_->enableReading(); // 让 Channel 对 wakeupFd_ 进行读事件的监听

    // 【新增 2】：配置 Timer Channel
    timerChannel_->setReadCallback(std::bind(&EventLoop::handleTimerRead, this)); // 设置心脏跳动的回调
    timerChannel_->enableReading(); // 让 Epoll 开始监听定时器！
}

EventLoop::~EventLoop() {
	wakeupChannel_->disableAll();
    wakeupChannel_->remove(); // 后面会完善这个接口，现在先不管
    ::close(wakeupFd_);

    // 【新增 3】：清理 Timer
    timerChannel_->disableAll();
    timerChannel_->remove();
    ::close(timerFd_);
}

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
        // 3. 处理任务队列 (比如主线程塞过来的 new Channel)
        // 即使没有 IO 事件，只要被 wakeup() 唤醒了，代码也会走到这里
        doPendingFunctors();
    }
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
}
// 在当前 Loop 线程执行任务
// 如果是当前线程 -> 立即执行
// 如果是其他线程 -> 排队 (queueInLoop)
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        // 如果是当前线程（比如 subLoop 自己调用的），直接执行
        cb();
    }
    else
    {   // 如果是其他线程（比如主线程调用的），排队
        queueInLoop(std::move(cb));
		// 放入队列
    }
}
void EventLoop::wakeup()
{
	uint64_t one = 1;
	ssize_t n = ::write(wakeupFd_, &one, sizeof one);// 向 eventfd 写数据，唤醒 Loop
    if (n != sizeof one) {
        perror("EventLoop::wakeup() writes wrong number of bytes");
	}
}
void EventLoop::addConnection(const TcpConnectionPtr& conn)
{
    connections_.insert(conn);
}//新增连接

void EventLoop::removeConnection(const TcpConnectionPtr& conn)
{
    connections_.erase(conn);
}//消除连接

// 【新增 4】：实现心脏跳动的回调函数
void EventLoop::handleTimerRead()
{
    uint64_t expirations;
    ssize_t n = ::read(timerFd_, &expirations, sizeof(expirations));
    if (n != sizeof(expirations)) {
        perror("EventLoop::handleTimerRead() reads wrong number of bytes");
    }

    std::cout << "\n[searching....]EventLoop " << threadId_
        << "searching for its" << connections_.size() << " connections..." << std::endl;

    time_t now = ::time(nullptr);
    int timeout_seconds = 5; // 设定：超过 10 秒没动静的，杀无赦！

    // 遍历生死簿，开启无情踢人模式
    // 【架构师避坑】：在 C++ 容器遍历中删除元素，一定要正确处理迭代器！
    for (auto it = connections_.begin(); it != connections_.end(); ) {
        TcpConnectionPtr conn = *it;

        if (now - conn->getLastActiveTime() > timeout_seconds) {
            std::cout << "  find zoobies connections! cut down!\n";

            // 1. 底层掐断 TCP 连接
            // (视你的网络库 API 而定，可能是 forceClose() 或 shutdown())
            // 这会导致底层触发 EPOLLHUP/EPOLLRDHUP，进入正常的销毁流程
            conn->forceClose();

            // 2. 从生死簿中彻底除名，并将迭代器安全地推向下一个
            it = connections_.erase(it);
        }
        else {
            // 这个客户还活着，检查下一个
            ++it;
        }
    }
}



void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);// 从 eventfd 读取数据，完成唤醒操作
    if (n != sizeof one) {
        perror("EventLoop::handleRead() reads wrong number of bytes");
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
		std::lock_guard<std::mutex> lock(mutex_);//加锁，保护任务队列
        pendingFunctors_.emplace_back(std::move(cb));// 把任务添加到队列
    }
    // 如果不是在当前线程，或者正在执行回调函数，则唤醒
    // 什么时候需要唤醒？
    // 1. 调用者不是当前线程 (isInLoopThread() == false) -> 必须唤醒，否则 Loop 还在睡觉
    // 2. 当前线程正在执行 pending functors (callingPendingFunctors_ == true) 
    //    -> 这是一个特殊情况：我在执行任务A的时候，任务A又 queue 了一个任务B。
    //       为了让任务B能尽快执行，我也再按一次铃（虽然不按也可能在下一次循环执行，但按了更保险）。
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);// 交换，减少锁的持有时间
    }
    // 这里执行任务时，不需要持有锁！
    // 这样做的好处：
    // 1. 即使某个任务执行很慢，也不会阻塞其他线程往 pendingFunctors_ 里塞新任务。
    // 2. 避免死锁（如果任务里又调用了 queueInLoop，如果还持锁就会死锁）。
    for (const Functor& func : functors)
    {
        func(); // 执行任务
    }
	callingPendingFunctors_ = false;
}




