#include <iostream>
#include<string>
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")  //����ws2_32.dll
#define NAME_LEN 20
using namespace std;

const int max_cli = 5;//�����������û�
bool flag = 0;  // ��־�Ƿ�����ʹ�ÿͻ��б�
int nSize = sizeof(SOCKADDR);
char all_send_Buffer[MAXBYTE + NAME_LEN + 3] = { 0 };  //����Ⱥ����Ϣ��server������clients���͵���Ϣ
char send_Buffer[MAXBYTE] = { 0 };//�ǵ��ȳ�ʼ��

struct client
{
	SOCKET clntSock;
	SOCKADDR clntAddr;
	char name[NAME_LEN] = { 0 };  // ����û������֣���ʼ��
	char receive_Buffer[MAXBYTE] = { 0 };  // server�����client�յ�����Ϣ
	char send_Buffer[MAXBYTE] = { 0 };  // server�����client���͵���Ϣ
	client* next = NULL;
};

struct clients_list
{
	client* root = NULL;
	client* end = NULL;
	int cur_size = 0;//��ǰ�û�����
}this_list;

void del(clients_list* a_list, client* b)//ɾ���û�
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
void insert(clients_list* a_list, client* b)//�����û�
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

DWORD WINAPI send_message(LPVOID lpParam)//��������ͻ���Ⱥ����Ϣ���߳�
{
	while (1)
	{
		if (cin.getline(send_Buffer, MAXBYTE))
		{
			while (flag)//δʹ���û��б�
				Sleep(100);
			flag = 1;//��Ϊ1��ʾ����ʹ���û��б�
			strcpy(all_send_Buffer, "[server]: ");//����
			strcat(all_send_Buffer, send_Buffer);//����
			for (client* cur = this_list.root; cur != NULL; cur = cur->next)
				send(cur->clntSock, all_send_Buffer, sizeof(all_send_Buffer), NULL);//��ʼ����
			flag = 0;//���ͽ�������Ϊ0
		}
	}
	return 0;
}

DWORD WINAPI receive_message(LPVOID lpParam)//������յ���Ϣ���߳�
{
	client* this_client = (client*)lpParam;
	recv(this_client->clntSock, this_client->name, 20, NULL);//�Ƚ����û���
	cout << "����Ϊ " << this_client->name << " ���û�����������" << endl;//��ӡ��ʾ���
	strcpy(all_send_Buffer, this_client->name);//��ֵ
	strcat(all_send_Buffer, "�ѽ���");//����
	while (flag)
		Sleep(100);
	for (client* cur = this_list.root; cur != NULL; cur = cur->next)
		send(cur->clntSock, all_send_Buffer, sizeof(all_send_Buffer), NULL);
	//��ͻ��˷�����Ϣ
	strcpy(this_client->send_Buffer, "��ã�");
	strcat(this_client->send_Buffer, this_client->name);//��ʾ����Ļ�ӭ���
	send(this_client->clntSock, this_client->send_Buffer, sizeof(this_client->send_Buffer), NULL);  // ��������, �������ݻ�����, ���ͻ������ĳ���, �Ե��õĴ���ʽ��
	strcpy(this_client->receive_Buffer, "\0");
	insert(&this_list, this_client);//����һ���û�
	flag = 0;
	while (1)
	{
		if (recv(this_client->clntSock, this_client->receive_Buffer, MAXBYTE, NULL) && strcmp(this_client->receive_Buffer, "\0"))
		{
			while (flag)
				Sleep(100);
			flag = 1;
			cout << this_client->name << ": " << this_client->receive_Buffer << endl;//��ӡ�û��Ͷ�Ӧ��Ϣ
			//�����������˳���ʽ����ɾ���û�
			if (strcmp(this_client->receive_Buffer, "q") == 0)
			{
				cout << "�û� [" << this_client->name << "] ���˳�" << endl;
				strcpy(all_send_Buffer, this_client->name);
				strcat(all_send_Buffer, "���˳�");
				for (client* cur = this_list.root; cur != NULL; cur = cur->next)
				{
					if (cur == this_client)
						continue;//�жϵ�ǰ�û��Ƿ���Ҫ�˳����û�
					send(cur->clntSock, all_send_Buffer, sizeof(all_send_Buffer), NULL);//���������������Ϣ
				}
				del(&this_list, this_client);//ɾ���û�
				closesocket(this_client->clntSock);//�ر��׽���
				delete this_client;
				flag = 0;
				break;
			}
			strcpy(all_send_Buffer, "[");
			strcat(all_send_Buffer, this_client->name);
			strcat(all_send_Buffer, "]: ");
			strcat(all_send_Buffer, this_client->receive_Buffer);//��ӡ�û��Ͷ�Ӧ��Ϣ
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
	//��ʼ��
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)  // makeword��һ���궨��(����ǿ������ת��)   ��������ϣ��ʹ�õ���߰汾, ����socket��ϸ��Ϣ��
	{
		cout << "socket�����ʧ��" << endl;
		//system("pause");
		return 0;
	}

	//�����׽���
	SOCKET servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  // ����ַ����, ��������, Э�飩

	//���׽���
	struct sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));		//ÿ���ֽ���0��䣬�ȳ�ʼ��
	sockAddr.sin_family = PF_INET;				//ʹ��ipv4
	sockAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");  // inet_addr()�����ѵ�ַת��Ϊ�ֽ���
	sockAddr.sin_port = htons(8888);			//�˿ڣ���Ҫ�����ж˿ںų�ͻ������Ҫ�Ϳͻ��˵Ķ˿ں���ͬ��  htons()����ת��Ϊ�����ֽ���
	// socket���ݽṹ��Ĭ����IP�Ͷ˿ںţ�ʹ��bind()Ϊ���IP�Ͷ˿ں�
	bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));  // ��������, ��ַ, ��ַ���ȣ�
	HANDLE send_hThread = CreateThread(NULL, NULL, send_message, NULL, 0, NULL);//����һ�����߳�
	CloseHandle(send_hThread);
	while (1)
	{
		//�������״̬
		listen(servSock, 5);  // (�׽���, �ȴ����Ӷ��е���󳤶�)  �����Ƿ������������Ŷ��ֵ���ʱ��ſ��Խ�������
		if (this_list.cur_size == max_cli)
			continue;
		client* this_client = new client;
		HANDLE* hThread = new HANDLE;
		this_client->clntSock = accept(servSock, (SOCKADDR*)&this_client->clntAddr, &nSize);  // ��������, Զ�˵�ַ, Զ�˵�ַ���ȵ�ָ�룩
	   // server�ᴴ��һ���µ�socket��client�����ӵ�����µ�socket�ϡ�accept()����ֵ���½���socket

		if (this_client->clntSock != INVALID_SOCKET)  // ������Ч������
		{
			*hThread = CreateThread(NULL, NULL, receive_message, this_client, 0, NULL);
			CloseHandle(*hThread);
		}
		else
		{
			closesocket(this_client->clntSock);//��Ч��ر��׽���ɾ���û�
			delete this_client;
		}
		delete hThread;
	}
	//�ر��׽���
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
	WSACleanup();//ֹͣDLLʹ��
	return 0;
}