#include "TcpConnection.h"

TcpConnection::TcpConnection(EventLoop* loop,
    const std::string& nameArg,
    int sockfd)
    : loop_(loop),
    name_(nameArg),
    state_(kConnecting),
    socketFd_(sockfd),
    channel_(new Channel(loop, sockfd)) // 创建 Channel
{
    // 【关键】设置 Channel 的回调
    // 当 Channel 发生事件时，调用 TcpConnection 自己的处理函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this)
    );
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    // setErrorCallback... (之后补)

    std::cout << "TcpConnection::ctor[" << name_ << "] at " << this
        << " fd=" << sockfd << std::endl;
    // 设置 KeepAlive 等 socket 选项 (可选)
    setNonBlock(socketFd_);
}

TcpConnection::~TcpConnection()
{
    std::cout << "TcpConnection::dtor[" << name_ << "] at " << this
        << " fd=" << socketFd_ << std::endl;
    ::close(socketFd_); // 析构时真正关闭 socket
}
// 供 Server 调用，通知连接建立完毕
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    // 1. 开启 Channel 的读监听 (enableReading)
    //    channel_->enableReading() 会调用 epoll_ctl
    channel_->enableReading();

    // 2. 回调用户的“新连接建立”函数
    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }
}

// 供 Server 调用，通知连接即将销毁
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll(); // 停止监听所有事件

        if (connectionCallback_) {
            connectionCallback_(shared_from_this());
        }
    }
    channel_->remove(); // 从 Loop 中移除
}

// 核心：读取数据
void TcpConnection::handleRead()
{
    int savedErrno = 0;
    // 1. 使用 Buffer 读取数据
    ssize_t n = inputBuffer_.readFd(socketFd_, &savedErrno);

    if (n > 0) {
        // 2. 如果读到了数据，回调用户的 MessageCallback
        //    把当前连接的 shared_ptr 和 Buffer 传给用户
        if (messageCallback_) {
            messageCallback_(shared_from_this(), &inputBuffer_, 0);
        }
    }
    else if (n == 0) {
        // 3. 读到 0，说明对端关闭
        handleClose();
    }
    else {
        // 4. 出错
        errno = savedErrno;
        std::cerr << "TcpConnection::handleRead error" << std::endl;
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    // 👇【新增日志 4】进入回调
    std::cout << "[Write] handleWrite triggered. Buffer remaining: " << outputBuffer_.readableBytes() << std::endl;
    // 只有关注了 EPOLLOUT 才有意义
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        // 注意：这里不用 outputBuffer_.readFd，因为我们要往 fd 写数据
        // 使用 peek() 拿到数据指针
        ssize_t n = ::write(socketFd_,
            outputBuffer_.peek(),
            outputBuffer_.readableBytes());

        if (n > 0)
        {
            // 既然发出去了一部分，就从 Buffer 里消费掉
            outputBuffer_.retrieve(n);
            std::cout << "handleWrite: wrote " << n << " bytes, remaining: " << outputBuffer_.readableBytes() <<std:: endl;
            if (outputBuffer_.readableBytes() == 0)
            {
                // Buffer 终于空了！
                // 停止关注 EPOLLOUT，否则 Epoll 会一直疯狂通知（Busy Loop）
                channel_->disableWriting();
                std::cout << "[Write] Buffer empty. Disable EPOLLOUT." << std::endl;

                if (writeCompleteCallback_)
                {
                    // loop_->queueInLoop(...)
                }

                // 考虑一种情况：正在 shutdown 时，Buffer 发完了，可以真正关闭了
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            // 👇【新增日志 7】写失败 (关键！)
            std::cout << "[Write] Error! n=" << n << " errno=" << errno << std::endl;
            // 如果是 EAGAIN，这是正常的，不要 panic
            if (errno != EAGAIN) {
                perror("TcpConnection::handleWrite");
            }
        }
    }
    else
    {
        std::cout << "Connection fd=" << socketFd_ << " is down, no more writing" << std::endl;
    }
}

void TcpConnection::handleClose()
{
    // std::cout << "fd = " << socketFd_ << " state = " << state_ << std::endl;
    setState(kDisconnected);
    channel_->disableAll();

    // 此时必须要持有一个指向自己的 shared_ptr，防止在回调过程中自己被析构
    // 这一步很重要：TcpConnection 需要通知 TcpServer 把自己从 map 中移除
    // 但目前还没实现 TcpServer 的移除接口，我们暂时留空，或者简单打印
    TcpConnectionPtr guardThis(shared_from_this());
    if (connectionCallback_) {
        connectionCallback_(guardThis); // 通知用户：连接断开了
    }
    // 【新增】通知 Server 移除自己
    // 必须最后调用，因为这行执行完，TcpConnection 对象可能就析构了
    if (closeCallback_) {
        closeCallback_(guardThis);
    }
}

void TcpConnection::handleError()
{
    int err = 0;
    // 获取 socket 错误信息... (省略细节)
    std::cerr << "TcpConnection::handleError [" << name_ << "] - SO_ERROR = " << err << std::endl;
}

// 发送接口 
void TcpConnection::send(const std::string& message)
{
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);
        }
        else {
            loop_->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, message)
            );
        }
    }
}

void TcpConnection::sendInLoop(const std::string& message)
{
    
    ssize_t nwrote = 0;
    size_t  remaining = message.size();
    bool  faultError = false;
    // 1. 如果 OutputBuffer 是空的，说明没有积压数据，尝试直接发送！
        //    (如果 OutputBuffer 不为空，说明之前就没发完，那这次也不能插队，必须先存 Buffer)
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(socketFd_, message.data(), remaining);
        // 👇【新增日志 1】直发情况
        if (nwrote > 0) {
            std::cout << "[Send] Direct write " << nwrote << " bytes." << std::endl;
        }
        if (nwrote >= 0)
        {
            remaining -= nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                // 如果一次全发完了，不需要再关注 EPOLLOUT
                // 可以在这里回调 "WriteComplete"（高水位回调，可选）
                //loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) // EWOULDBLOCK 就是 EAGAIN，不是真错误
            {
                perror("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // 对方断开了
                {
                    faultError = true;
                }
            }
        }
    }

    // 2. 如果还有剩余没发完（或者压根没机会直发）
    //    并且没有发生致命错误，就把剩下的数据存入 OutputBuffer
    if (!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes();
        // 把剩下的存起来
        outputBuffer_.append(message.data() + nwrote, remaining);
        // 👇【新增日志 2】Buffer 积压情况
        std::cout << "[Send] Buffer append " << remaining << " bytes. "
            << "Buffer size: " << oldLen << " -> " << outputBuffer_.readableBytes() << std::endl;

        // 【关键】告诉 Epoll：我不行了，等管子通了（EPOLLOUT）叫我！
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
            std::cout << "[Send] Enable EPOLLOUT (isWriting=true)" << std::endl;
        }
    }
}



void TcpConnection::shutdown()
{
    // 暂时简写
    if (state_ == kConnected) {
        setState(kDisconnecting);
        // loop_->runInLoop(...)
    }
}
void TcpConnection::shutdownInLoop() {
    // ::shutdown(socketFd_, SHUT_WR);
}


// 辅助函数：设置非阻塞
void TcpConnection::setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}