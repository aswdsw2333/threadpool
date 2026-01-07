#ifndef TCPSERVER_H
#define TCPSERVER_H
#include<iostream>
#include<string>
#include<cstring>
#include<thread>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<cerrno>

// Include Clientsession implementation (you can change to include header in your project)
#include "Clientsession.h"
using namespace std;

class Tcpserver
{
private:
	int server_fd; // communication socket
	int server_port; // server port
public:
	Tcpserver(int port);
	~Tcpserver();
	void Start()
	{
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in server_add{};
		server_add.sin_family = AF_INET;
		server_add.sin_port = htons(server_port);
		server_add.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
		// enable address reuse
		int opt = 1;
		setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		if (bind(server_fd, (sockaddr*)&server_add, sizeof(server_add)) < 0)
		{
			cout << "Bind failed" << endl;
			return;
		}
		if (listen(server_fd, 5) < 0)
		{
			cout << "Listen failed" << endl;
			return;
		}
		cout << "Server started, waiting for client connections..." << endl;
		while (true)
		{
			sockaddr_in client_add{};
			socklen_t client_len = sizeof(client_add);
			int client_fd = accept(server_fd, (sockaddr*)&client_add, &client_len);
			if (client_fd == -1)
			{
				cout << "Failed to accept connection" << endl;
				continue;
			}
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &client_add.sin_addr, ip_str, sizeof(ip_str));
			cout << "Client connected, IP: " << ip_str << " Port: " << ntohs(client_add.sin_port) << endl;
			// create a session object for each client and start the session
			Clientsession* session = new Clientsession(client_fd, string(ip_str));
			thread([session]
				{
					session->start();
					delete session; // release resources after session ends
				}).detach();
		}
	}
};
#endif // !TCPSERVER_H
