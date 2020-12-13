#include <iostream>
#include<string>
#include <WinSock2.h>
#include<windows.h>
#pragma comment(lib,"ws2_32.lib")
#define NAME_LEN 20
using namespace std;

struct connect_info
{
	SOCKET clntSock;
	sockaddr_in sockAddr;
	char name[NAME_LEN] = { 0 };//用户名字
	char receiver[NAME_LEN] = { 0 };//接收者
	char receive_Buffer[MAXBYTE] = { 0 };  // 这个client收到的信息
	char send_Buffer[MAXBYTE] = { 0 };  // 这个client发送的信息
	bool exit;
}this_connect_info;

DWORD WINAPI send_message(LPVOID lpParam)//发送信息的线程
{
	connect_info* this_connect = (connect_info*)lpParam;
	while (1)
	{
		cout << " 输入“q”退出" << endl;//首先的退出的提示语句
		if (cin.getline(this_connect->receiver, NAME_LEN))
		{
			if (send(this_connect->clntSock, this_connect->receiver, sizeof(this_connect->receiver), NULL) == SOCKET_ERROR)
			{
				cout << "发送信息失败了！" << endl;
				continue;
			}
			cout << "请输入你想要发送的信息: ";
			if (cin.getline(this_connect->send_Buffer, MAXBYTE))//输入的信息
			{
				if (send(this_connect->clntSock, this_connect->send_Buffer, sizeof(this_connect->send_Buffer), NULL) == SOCKET_ERROR)
				{
					cout << "发送信息到服务器失败了！" << endl;
					continue;
				}
				cout << "已发送到服务器！" << endl;
				if (strcmp(this_connect->send_Buffer, "q") == 0)
				{
					this_connect->exit = 1;
					break;
				}
			}
		}
	}
	return 0;
}

DWORD WINAPI receive_message(LPVOID lpParam)//收到信息的线程
{
	connect_info* this_connect = (connect_info*)lpParam;
	while (1)
	{
		if (this_connect->exit)
			break;
		if (!this_connect->exit && recv(this_connect->clntSock, this_connect->receive_Buffer, MAXBYTE, NULL) && strcmp(this_connect->receive_Buffer, "\0"))
		{
			cout << this_connect->receive_Buffer << endl;//未退出且收到信息就打印出来
			strcpy(this_connect->receive_Buffer, "\0");
		}
	}
	return 0;
}
//创建两个线程，一个发一个收
DWORD  dwThreadID[2];
HANDLE hThread[2];

int main()  // int argv, char* argc[]
{
	//初始化
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "载入socket库失败" << endl;
		system("pause");
		return 0;
	}
	//创建套接字
	this_connect_info.clntSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	//向服务器发送消息
	memset(&this_connect_info.sockAddr, 0, sizeof(this_connect_info.sockAddr));//每个字节都用0填充
	this_connect_info.sockAddr.sin_family = PF_INET;
	this_connect_info.sockAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	this_connect_info.sockAddr.sin_port = htons(8888);
	this_connect_info.exit = 0;
	if (connect(this_connect_info.clntSock, (SOCKADDR*)&this_connect_info.sockAddr, sizeof(SOCKADDR)) != SOCKET_ERROR)  // （描述符, 服务器地址, 地址长度）
	{
		// 向服务端发送消息
		cout << "请输入用户名：";
		cin.getline(this_connect_info.send_Buffer, MAXBYTE);
		strcpy(this_connect_info.name, this_connect_info.send_Buffer);
		send(this_connect_info.clntSock, this_connect_info.send_Buffer, sizeof(this_connect_info.send_Buffer), NULL);

		// 接收服务器消息
		recv(this_connect_info.clntSock, this_connect_info.receive_Buffer, MAXBYTE, NULL);
		//输出接收到的数据
		cout << "[server]：" << this_connect_info.receive_Buffer << endl;
		strcpy(this_connect_info.receive_Buffer, "\0");

		hThread[0] = CreateThread(NULL, NULL, send_message, &this_connect_info, 0, &dwThreadID[0]);
		hThread[1] = CreateThread(NULL, NULL, receive_message, &this_connect_info, 0, &dwThreadID[1]);
		//只是关闭了一个线程句柄对象，表示我不再使用该句柄，即不对这个句柄对应的线程做任何干预了。并没有结束线程。
		CloseHandle(hThread[0]);

		DWORD result;
		while (1)
			/*获得线程的退出码，  
			第二个参数是一个 DWORD的指针，
			用户应该使用一个 DWORD 类型的变量去接收数据，
			返回的数据是线程的退出码，
			第一个参数是线程句柄，用 CreateThread 创建线程时获得到。  
			通过线程退出码可以判断线程是否正在运行，还是已经退出。  
			或者可以判断线程是否是正常退出还是异常退出。  */
			if (GetExitCodeThread(hThread[1], &result) && result != STILL_ACTIVE)
				break;
		Sleep(100);
		CloseHandle(hThread[1]);
	}
	//关闭套接字
	closesocket(this_connect_info.clntSock);
	WSACleanup();//停止DLL
	return 0;
}
