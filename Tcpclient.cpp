#include"Tcpclient.h"
using namespace std;

	Tcpclient::Tcpclient(string ip,int port):server_ip(ip),server_port(port), client_fd(-1) {}
	Tcpclient::~Tcpclient()
	{
		if(client_fd!=-1)
			close(client_fd);
	}
	