
#include"Clientsession.h"
std::mutex mtx;
Clientsession::Clientsession(int fd, string ip) :client_fd(fd), client_ip(ip) {};
Clientsession::~Clientsession()
	{
		if (client_fd != -1)
			close(client_fd);
	}
