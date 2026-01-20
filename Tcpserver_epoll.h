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
using namespace std;
#define MAX_EVENTS 10
#define PORT 5005
#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 20
#define MAX_THREAD_NUMS 4
// 用来管理所有 new 出来的 Channel，防止内存泄漏
// key: fd, value: Channel*
// 
// ✅ 改成 extern 声明
extern std::map<int, std::shared_ptr<Channel>> connectionMap;
extern std::mutex coutMutex;
extern std::map<int, Channel*> channel_map;

// 设置 socket 为非阻塞模式 (关键！)
class Tcpserver_epoll
{
public:
	Tcpserver_epoll(int port);
	~Tcpserver_epoll();
	void start();
	// [新增 2] 设置线程数量的接口
	void setThreadNum(int numThreads) { threadPool_->setThreadNum(numThreads); }
private:
	void setnonblocking(int sockfd);
	int listen_fd;
	sockaddr_in server_add;
	int port;
	int max_connect_;
	int max_threadnums;
	EventLoop loop; // 主 Loop (Main Reactor)

	// [新增 3] 线程池指针
	std::unique_ptr<EventLoopThreadPool> threadPool_;

};

