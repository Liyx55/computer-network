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
	char name[NAME_LEN] = { 0 };//�û�����
	char receiver[NAME_LEN] = { 0 };//������
	char receive_Buffer[MAXBYTE] = { 0 };  // ���client�յ�����Ϣ
	char send_Buffer[MAXBYTE] = { 0 };  // ���client���͵���Ϣ
	bool exit;
}this_connect_info;

DWORD WINAPI send_message(LPVOID lpParam)//������Ϣ���߳�
{
	connect_info* this_connect = (connect_info*)lpParam;
	while (1)
	{
		cout << " ���롰q���˳�" << endl;//���ȵ��˳�����ʾ���
		if (cin.getline(this_connect->receiver, NAME_LEN))
		{
			if (send(this_connect->clntSock, this_connect->receiver, sizeof(this_connect->receiver), NULL) == SOCKET_ERROR)
			{
				cout << "������Ϣʧ���ˣ�" << endl;
				continue;
			}
			cout << "����������Ҫ���͵���Ϣ: ";
			if (cin.getline(this_connect->send_Buffer, MAXBYTE))//�������Ϣ
			{
				if (send(this_connect->clntSock, this_connect->send_Buffer, sizeof(this_connect->send_Buffer), NULL) == SOCKET_ERROR)
				{
					cout << "������Ϣ��������ʧ���ˣ�" << endl;
					continue;
				}
				cout << "�ѷ��͵���������" << endl;
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

DWORD WINAPI receive_message(LPVOID lpParam)//�յ���Ϣ���߳�
{
	connect_info* this_connect = (connect_info*)lpParam;
	while (1)
	{
		if (this_connect->exit)
			break;
		if (!this_connect->exit && recv(this_connect->clntSock, this_connect->receive_Buffer, MAXBYTE, NULL) && strcmp(this_connect->receive_Buffer, "\0"))
		{
			cout << this_connect->receive_Buffer << endl;//δ�˳����յ���Ϣ�ʹ�ӡ����
			strcpy(this_connect->receive_Buffer, "\0");
		}
	}
	return 0;
}
//���������̣߳�һ����һ����
DWORD  dwThreadID[2];
HANDLE hThread[2];

int main()  // int argv, char* argc[]
{
	//��ʼ��
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "����socket��ʧ��" << endl;
		system("pause");
		return 0;
	}
	//�����׽���
	this_connect_info.clntSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	//�������������Ϣ
	memset(&this_connect_info.sockAddr, 0, sizeof(this_connect_info.sockAddr));//ÿ���ֽڶ���0���
	this_connect_info.sockAddr.sin_family = PF_INET;
	this_connect_info.sockAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	this_connect_info.sockAddr.sin_port = htons(8888);
	this_connect_info.exit = 0;
	if (connect(this_connect_info.clntSock, (SOCKADDR*)&this_connect_info.sockAddr, sizeof(SOCKADDR)) != SOCKET_ERROR)  // ��������, ��������ַ, ��ַ���ȣ�
	{
		// �����˷�����Ϣ
		cout << "�������û�����";
		cin.getline(this_connect_info.send_Buffer, MAXBYTE);
		strcpy(this_connect_info.name, this_connect_info.send_Buffer);
		send(this_connect_info.clntSock, this_connect_info.send_Buffer, sizeof(this_connect_info.send_Buffer), NULL);

		// ���շ�������Ϣ
		recv(this_connect_info.clntSock, this_connect_info.receive_Buffer, MAXBYTE, NULL);
		//������յ�������
		cout << "[server]��" << this_connect_info.receive_Buffer << endl;
		strcpy(this_connect_info.receive_Buffer, "\0");

		hThread[0] = CreateThread(NULL, NULL, send_message, &this_connect_info, 0, &dwThreadID[0]);
		hThread[1] = CreateThread(NULL, NULL, receive_message, &this_connect_info, 0, &dwThreadID[1]);
		//ֻ�ǹر���һ���߳̾�����󣬱�ʾ�Ҳ���ʹ�øþ������������������Ӧ���߳����κθ�Ԥ�ˡ���û�н����̡߳�
		CloseHandle(hThread[0]);

		DWORD result;
		while (1)
			/*����̵߳��˳��룬  
			�ڶ���������һ�� DWORD��ָ�룬
			�û�Ӧ��ʹ��һ�� DWORD ���͵ı���ȥ�������ݣ�
			���ص��������̵߳��˳��룬
			��һ���������߳̾������ CreateThread �����߳�ʱ��õ���  
			ͨ���߳��˳�������ж��߳��Ƿ��������У������Ѿ��˳���  
			���߿����ж��߳��Ƿ��������˳������쳣�˳���  */
			if (GetExitCodeThread(hThread[1], &result) && result != STILL_ACTIVE)
				break;
		Sleep(100);
		CloseHandle(hThread[1]);
	}
	//�ر��׽���
	closesocket(this_connect_info.clntSock);
	WSACleanup();//ֹͣDLL
	return 0;
}
