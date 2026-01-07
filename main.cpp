#include"Tcpserver.h"
#include"Clientsession.h"
#include"Thread_pool.h"
#include"Tcpclient.h"
using namespace std;

int main()
{
	Tcpserver server(5005);
	thread server_thread([&server]() {
		server.Start();
		});
	this_thread::sleep_for(chrono::seconds(1)); // ensure server starts first
	const int COUNT_CLIENT = 5;
	vector<thread> Client_threads;
	for (int i = 0; i < COUNT_CLIENT; i++)
	{
		Client_threads.emplace_back([i]()
			{
				string server_ip = "127.0.0.1";
				Tcpclient client(server_ip, 5005);//在线程函数，该类对象执行完毕后自动析构
				cout << "Client " << i << " attempting to connect to server..." << endl;
				if (!client.connect())
				{
					cout << "Failed to connect to server" << endl;
					return;
				}
				string msg = "Hello " + to_string(i);
				client.send_data(msg);

				// 3. 关键点：接收服务器响应
				// 对于第 5 个客户端，这行代码将会被阻塞！
				string response = client.recv();
				cout << "[Client " << i << "] Got Response: " << response << endl;

				// 4. 模拟长连接：让前 4 个客户端占住线程不放
				// 这样第 5 个客户端就必须在队列里等
				this_thread::sleep_for(chrono::seconds(10));

				
				// socket will be closed when client is destructed
			});

	}

	// wait for all clients to finish
	for (auto& t : Client_threads) 
		if (t.joinable()) t.join();

	// The example lets the server keep running; to stop, implement a stop flag and close server_fd
	// Here we simply wait and exit (note: server.Start() currently blocks in accept loop)
	this_thread::sleep_for(chrono::seconds(1));
	// To terminate server thread before program exit, implement shutdown logic in Tcpserver (recommended)
	// For now detach or let program end (note: server thread will continue running)
	if (server_thread.joinable()) server_thread.detach();

	return 0;
}
