#include "Buffer.h"






 Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
    readerIndex_(kCheapPrepend),
    writerIndex_(kCheapPrepend)
{
}


void Buffer:: retrieve(size_t len) {
    assert(len <= readableBytes());
    if (len < readableBytes()) {
        readerIndex_ += len;
    }
    else {
        retrieveAll();
    }
}



void Buffer:: retrieveAll() {
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
}



// 把所有数据转成 string 返回（用于调试或简单业务）
std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}


std::string Buffer::retrieveAsString(size_t len) {
    assert(len <= readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}


// 核心功能：写入数据
void  Buffer::append(const std::string& str) {
    append(str.data(), str.size());
}

void Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    hasWritten(len);
}

// 确保有足够的空间写
void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);
    }
    assert(writableBytes() >= len);
}

ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    // 64k 栈空间，用作临时缓冲区
    // 栈内存分配极快（移动栈指针而已），且函数退出自动销毁
    char extrabuff[65536];
    struct iovec vec[2];

    ssize_t sum_n = 0;
    while (true)
    {
        const size_t writable = writableBytes();//可以写多少
        vec[0].iov_base = beginWrite();
        vec[0].iov_len = writable;
        //第二块缓冲区
        vec[1].iov_base = extrabuff;//返回指针
        vec[1].iov_len = sizeof(extrabuff);
        // 如果 Buffer 内部空间本身就很大（比如已经扩容到 1MB 了，且剩余空间 > 64KB）
        // 那就不需要用 extrabuf 了，直接读到 Buffer 里就行。
        // 但通常情况下（初始 1024 bytes），我们需要这两个缓冲区。
        const int iovcnt = (writable < sizeof(extrabuff)) ? 2 : 1;
        const ssize_t n = readv(fd, vec, iovcnt);//关键系统调用readv
        
            if (n < 0) {
                // 🚨 关键时刻：EAGAIN 表示“读空了”
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break; // 任务完成，可以退出了
                }
                // 真错误
                *saveErrno = errno;
                break;
            }
        
        else if (static_cast<size_t>(n) <= writable) {
            // 情况 A：数据量小，Buffer 内部装得下
            writerIndex_ += n;
            sum_n += n;
        }
        else {
            // 情况 B：数据量大，Buffer 装满了，剩下的溢出到了 extrabuf
            writerIndex_ = buffer_.size(); // 先把内部指针移到最后
            sum_n += n;
            // 把 extrabuf 里的剩余数据追加到 Buffer 末尾
            // 这一步会触发 makeSpace -> resize
            append(extrabuff, n - writable);
        }

        
    }
    return sum_n;
}



void Buffer::makeSpace(size_t len)
{
    
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        // 真的不够了，扩容
        buffer_.resize(writerIndex_ + len);
    }
    else {
        // 够用，但是碎片化了，内部把数据挪到最前面
        size_t readable = readableBytes();
        std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;
    }

}