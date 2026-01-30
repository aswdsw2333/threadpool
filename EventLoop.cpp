
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

EventLoop::EventLoop()
    : looping_(false), 
    quit_(false), 
    epoll_(new Epoll()),// 创建 Epoll 对象
	wakeupFd_(creatEventfd()), // 创建 eventfd
	threadId_(std::this_thread::get_id()), // 记录当前线程 ID
	callingPendingFunctors_(false),
	wakeupChannel_(new Channel(this, wakeupFd_)) // 创建用于监听 wakeupFd_ 的 Channel
{
	wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));// 设置回调：当有人按铃时，执行 handleRead 把数据读走
	wakeupChannel_->enableReading(); // 让 Channel 对 wakeupFd_ 进行读事件的监听
}

EventLoop::~EventLoop() {
	wakeupChannel_->disableAll();
    wakeupChannel_->remove(); // 后面会完善这个接口，现在先不管
    ::close(wakeupFd_);
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

