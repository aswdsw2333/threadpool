#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <sys/uio.h>
#include <cstring>      // 提供 memcpy


class Buffer {
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize);


    // 可读字节数
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }//表示该函数不会修改当前类的任何成员变量（buffer_ 的值不会被这个函数改变），且只能被常量对象调用。
    // 可写字节数
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    // 头部预留字节数
    size_t prependableBytes() const { return readerIndex_; }

    // 返回可读数据的指针
    const char* peek() const { return begin() + readerIndex_; }
    int32_t peekInt32() const;
    int32_t readInt32();

    // 核心功能：取走数据（逻辑上取走，移动 readerIndex）
    void retrieve(size_t len);

    // 取走所有数据，复位
    void retrieveAll();

    // 把所有数据转成 string 返回（用于调试或简单业务）
    std::string retrieveAllAsString();

    std::string retrieveAsString(size_t len);

    // 核心功能：写入数据
    void append(const std::string& str);

    void append(const char* data, size_t len);


    void appendInt32(int32_t x);
    // 确保有足够的空间写
    void ensureWritableBytes(size_t len);

    // 返回可写位置的指针
    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    // 写完后移动指针
    void hasWritten(size_t len) { writerIndex_ += len; }

    // 【重要】从 fd 读取数据 (结合 readv 优化)
    ssize_t readFd(int fd, int* saveErrno);

private:
    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }
    //函数返回值是「指向 const char 的指针」，意味着调用者只能读取指针指向的内容，不能修改（只读）；
    // 扩容或整理空间
    void makeSpace(size_t len);


    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

