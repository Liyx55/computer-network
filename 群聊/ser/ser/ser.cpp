#include <iostream>
#include<string>
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")  //加载ws2_32.dll
#define NAME_LEN 20
using namespace std;

const int max_cli = 5;//设置最多五个用户
bool flag = 0;  // 标志是否正在使用客户列表
int nSize = sizeof(SOCKADDR);
char all_send_Buffer[MAXBYTE + NAME_LEN + 3] = { 0 };  //定义群发消息，server向所有clients发送的信息
char send_Buffer[MAXBYTE] = { 0 };//记得先初始化

struct client
{
	SOCKET clntSock;
	SOCKADDR clntAddr;
	char name[NAME_LEN] = { 0 };  // 这个用户的名字！初始化
	char receive_Buffer[MAXBYTE] = { 0 };  // server从这个client收到的信息
	char send_Buffer[MAXBYTE] = { 0 };  // server向这个client发送的信息
	client* next = NULL;
};

struct clients_list
{
	client* root = NULL;
	client* end = NULL;
	int cur_size = 0;//当前用户数量
}this_list;

void del(clients_list* a_list, client* b)//删除用户
{
	client* cur = a_list->root;
	if (cur == b)
	{
		a_list->cur_size--;
		a_list->root = cur->next;
		return;
	}
	for (; cur != NULL; cur = cur->next)
		if (cur->next == b)
		{
			a_list->cur_size--;
			if (cur->next->next == NULL)
				a_list->end = cur;
			cur->next = cur->next->next;
			break;
		}
}
void insert(clients_list* a_list, client* b)//增加用户
{
	if (a_list->end == NULL)
	{
		a_list->root = b;
		a_list->end = b;
	}
	else
	{
		a_list->end->next = b;
		a_list->end = b;
	}
	a_list->cur_size++;
	return;
}

DWORD WINAPI send_message(LPVOID lpParam)//服务器向客户端群发消息的线程
{
	while (1)
	{
		if (cin.getline(send_Buffer, MAXBYTE))
		{
			while (flag)//未使用用户列表
				Sleep(100);
			flag = 1;//置为1表示正在使用用户列表
			strcpy(all_send_Buffer, "[server]: ");//复制
			strcat(all_send_Buffer, send_Buffer);//连接
			for (client* cur = this_list.root; cur != NULL; cur = cur->next)
				send(cur->clntSock, all_send_Buffer, sizeof(all_send_Buffer), NULL);//开始发送
			flag = 0;//发送结束后置为0
		}
	}
	return 0;
}

DWORD WINAPI receive_message(LPVOID lpParam)//服务端收到消息的线程
{
	client* this_client = (client*)lpParam;
	recv(this_client->clntSock, this_client->name, 20, NULL);//先接收用户名
	cout << "姓名为 " << this_client->name << " 的用户进入聊天室" << endl;//打印提示语句
	strcpy(all_send_Buffer, this_client->name);//赋值
	strcat(all_send_Buffer, "已进入");//连接
	while (flag)
		Sleep(100);
	for (client* cur = this_list.root; cur != NULL; cur = cur->next)
		send(cur->clntSock, all_send_Buffer, sizeof(all_send_Buffer), NULL);
	//向客户端发送消息
	strcpy(this_client->send_Buffer, "你好！");
	strcat(this_client->send_Buffer, this_client->name);//提示最初的欢迎语句
	send(this_client->clntSock, this_client->send_Buffer, sizeof(this_client->send_Buffer), NULL);  // （描述符, 发送数据缓冲区, 发送缓冲区的长度, 对调用的处理方式）
	strcpy(this_client->receive_Buffer, "\0");
	insert(&this_list, this_client);//增加一个用户
	flag = 0;
	while (1)
	{
		if (recv(this_client->clntSock, this_client->receive_Buffer, MAXBYTE, NULL) && strcmp(this_client->receive_Buffer, "\0"))
		{
			while (flag)
				Sleep(100);
			flag = 1;
			cout << this_client->name << ": " << this_client->receive_Buffer << endl;//打印用户和对应信息
			//设置正常的退出方式，并删除用户
			if (strcmp(this_client->receive_Buffer, "q") == 0)
			{
				cout << "用户 [" << this_client->name << "] 已退出" << endl;
				strcpy(all_send_Buffer, this_client->name);
				strcat(all_send_Buffer, "已退出");
				for (client* cur = this_list.root; cur != NULL; cur = cur->next)
				{
					if (cur == this_client)
						continue;//判断当前用户是否是要退出的用户
					send(cur->clntSock, all_send_Buffer, sizeof(all_send_Buffer), NULL);//不是则继续发送信息
				}
				del(&this_list, this_client);//删除用户
				closesocket(this_client->clntSock);//关闭套接字
				delete this_client;
				flag = 0;
				break;
			}
			strcpy(all_send_Buffer, "[");
			strcat(all_send_Buffer, this_client->name);
			strcat(all_send_Buffer, "]: ");
			strcat(all_send_Buffer, this_client->receive_Buffer);//打印用户和对应信息
			for (client* cur = this_list.root; cur != NULL; cur = cur->next)
			{
				if (cur == this_client)
					continue;
				send(cur->clntSock, all_send_Buffer, sizeof(all_send_Buffer), NULL);
			}
			flag = 0;
		}
	}
	return 0;
}


int main()  // int argv, char* argc[]
{
	//初始化
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)  // makeword是一个宏定义(进行强制类型转换)   （调用者希望使用的最高版本, 可用socket详细信息）
	{
		cout << "socket库加载失败" << endl;
		//system("pause");
		return 0;
	}

	//创建套接字
	SOCKET servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  // （地址类型, 服务类型, 协议）

	//绑定套接字
	struct sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));		//每个字节用0填充，先初始化
	sockAddr.sin_family = PF_INET;				//使用ipv4
	sockAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");  // inet_addr()函数把地址转换为字节序
	sockAddr.sin_port = htons(8888);			//端口（不要和现有端口号冲突，并且要和客户端的端口号相同）  htons()函数转换为网络字节序
	// socket数据结构中默认有IP和端口号，使用bind()为其绑定IP和端口号
	bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));  // （描述符, 地址, 地址长度）
	HANDLE send_hThread = CreateThread(NULL, NULL, send_message, NULL, 0, NULL);//创建一个新线程
	CloseHandle(send_hThread);
	while (1)
	{
		//进入监听状态
		listen(servSock, 5);  // (套接字, 等待连接队列的最大长度)  监听是否有连接请求，排队轮到的时候才可以进行连接
		if (this_list.cur_size == max_cli)
			continue;
		client* this_client = new client;
		HANDLE* hThread = new HANDLE;
		this_client->clntSock = accept(servSock, (SOCKADDR*)&this_client->clntAddr, &nSize);  // （描述符, 远端地址, 远端地址长度的指针）
	   // server会创建一个新的socket，client会连接到这个新的socket上。accept()返回值是新建的socket

		if (this_client->clntSock != INVALID_SOCKET)  // 不是无效的数据
		{
			*hThread = CreateThread(NULL, NULL, receive_message, this_client, 0, NULL);
			CloseHandle(*hThread);
		}
		else
		{
			closesocket(this_client->clntSock);//无效则关闭套接字删除用户
			delete this_client;
		}
		delete hThread;
	}
	//关闭套接字
	client* before = NULL;
	for (client* cur = this_list.root; cur != NULL; cur = cur->next)
	{
		closesocket(cur->clntSock);
		if (before != NULL)
			delete before;
		before = cur;
	}
	delete before;
	closesocket(servSock);
	WSACleanup();//停止DLL使用
	return 0;
}