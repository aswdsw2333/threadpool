
// 包含 Clientsession 实现（你的工程中也可以改为包含头文件）
#include "Tcpserver.h"
using namespace std;

Tcpserver::Tcpserver(int port,int thread_count):server_fd(-1),server_port(port),tp(thread_count) {}
Tcpserver::~Tcpserver()
	{
		if(server_fd!=-1)
			close(server_fd);
	}
	