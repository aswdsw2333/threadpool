
// 包含 Clientsession 实现（你的工程中也可以改为包含头文件）
#include "Tcpserver.h"
using namespace std;

Tcpserver::Tcpserver(int port):server_fd(-1),server_port(port){}
Tcpserver::~Tcpserver()
	{
		if(server_fd!=-1)
			close(server_fd);
	}
	