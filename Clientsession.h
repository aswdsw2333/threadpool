#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H	

#include <iostream> // add this header to declare cout and endl
#include<string>
#include<thread>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<vector>
#include<cstring>  // header for memset, strlen, etc.
#include<mutex>    // thread-safety: mutex
using namespace std;

extern mutex mtx;
class Clientsession
{
private:
	int client_fd; // communication socket
	string client_ip; // client IP
	bool send_data(const string& data)
	{
		if (client_fd == -1) return false;
		const char* data_ptr = data.c_str();
		size_t total_bytes = data.size();
		size_t sent_len = 0;
		while (sent_len < total_bytes)
		{
			ssize_t ret = send(client_fd, data_ptr + sent_len, total_bytes - sent_len, 0);
			if (ret <= 0)
			{
				cout << "Failed to send data to client " << client_ip << endl;
				return false;
			}
			sent_len += ret;
		}
		return true;
	}
public:
	Clientsession(int fd, string ip);
	~Clientsession();
	void start()
	{
		char buffer[1024] = { 0 };

		while (true)
		{
			// Clear buffer before each receive
			memset(buffer, 0, sizeof(buffer));
			ssize_t bytes_rec = recv(client_fd, &buffer[0], sizeof(buffer), 0);
			if (bytes_rec <= 0)
			{
				cout << "Client " << client_ip << " disconnected" << endl;
				break;
			}
			string msg(buffer);
			unique_lock<mutex> lock(mtx); // lock to protect output operations
			{
				cout << "Client " << client_ip << " socket:" << client_fd << " says " << msg << endl;
			}
			string echo_msg = "Server has received your message: " + msg;
			if (!send_data(echo_msg)) {
				break;
			}

		}
		// Close connection and mark
		if (client_fd != -1) {
			close(client_fd);
			client_fd = -1;
		}
	}
}; 
#endif // !CLIENTSESSION_H