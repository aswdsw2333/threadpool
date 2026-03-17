#pragma once
#include<iostream>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<unistd.h>
#include <mutex>
#include<string>
#include<arpa/inet.h>
#include<vector>
#include<cstring>
#include<fcntl.h>
#include"Channel.h"
#include"epoll.h"
#include"EventLoop.h"
#include <map>           // 用于管理 Channel 对象
#include <errno.h>
#include "EventLoopThreadPool.h"
#include"Buffer.h"
#include"TcpConnection.h"

using namespace std;
#define MAX_EVENTS 10
#define PORT 5005
#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 20
#define MAX_THREAD_NUMS 4
// 设置 socket 为非阻塞模式 (关键！)
class Tcpserver_epoll
{
public:
	Tcpserver_epoll(int port);
	~Tcpserver_epoll();
	void start();
	// [新增 2] 设置线程数量的接口
	void setThreadNum(int numThreads) { threadPool_->setThreadNum(numThreads); }
	// 【新增】设置用户回调的接口（透传给 TcpConnection）
	void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
	void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
private:
	void setnonblocking(int sockfd);
	// 【新增】每当有一个新连接，就调用这个函数
	void newConnection(int sockfd);
	// 【新增】当连接断开时，TcpConnection 会回调这个函数
	void removeConnection(const TcpConnectionPtr& conn);
	void removeConnectionInLoop(const TcpConnectionPtr& conn);
	int listen_fd;
	sockaddr_in server_add;
	int port;
	int max_connect_;
	int max_threadnums;
	// 【新增】连接列表
	// key: 连接名称 (String), value: 连接对象 (shared_ptr)
	std::map<std::string, TcpConnectionPtr> connections_;

	// 【新增】为了生成唯一的连接名称 (如 "Conn-1", "Conn-2")
	int nextConnId_ = 1;
	EventLoop loop; // 主 Loop (Main Reactor)
	MessageCallback messageCallback_; // 用户的消息回调函数
	ConnectionCallback connectionCallback_; // 用户的连接回调函数

	// [新增 3] 线程池指针
	std::unique_ptr<EventLoopThreadPool> threadPool_;

};

