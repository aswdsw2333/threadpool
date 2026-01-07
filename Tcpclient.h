#ifndef TCPCLIENT_H
#define TCPCLIENT_H
#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;
class Tcpclient
{
private:
	int client_fd;
	string server_ip;
	int server_port; // server port

public:
	Tcpclient(string ip, int port);
	~Tcpclient();
	bool connect()
	{
		client_fd = socket(AF_INET, SOCK_STREAM, 0); // specify protocol; 0 lets OS pick the appropriate one
		if (client_fd == -1) return false;
		sockaddr_in server_add{};
		server_add.sin_family = AF_INET;
		server_add.sin_port = htons(server_port);
		// Convert string IP to binary
		if (inet_pton(AF_INET, server_ip.c_str(), &server_add.sin_addr) <= 0)
			return false;
		if (::connect(client_fd, (sockaddr*)&server_add, sizeof(server_add))< 0)
			return false;

		return true;
	}
	void send_data(const string& data)
	{
		if (client_fd == -1) return;
		ssize_t bytes_sent = send(client_fd, data.c_str(), data.size(), 0);
	}

	string recv()
	{
		if (client_fd == -1) return "Receive error";
		char buffer[1024] = { 0 };
		ssize_t bytes_rec = ::recv(client_fd, &buffer[0], sizeof(buffer) - 1, 0);
		if (bytes_rec <= 0)
			return "Receive error";
		return string(buffer);
	}
};
#endif // TCPCLIENT_H
