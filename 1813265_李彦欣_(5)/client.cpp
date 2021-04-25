#include<Winsock2.h>
#include<vector>
#include<stdio.h>
#include<string.h>
#include<iostream> 
#include<algorithm>
#include<time.h>
#include<cstdlib>
#include <fstream>
#pragma comment(lib,"ws2_32")
using namespace std;
#define SERVER_PORT 4001//·����ת���Ķ˿�
#define CLIENT_PORT 6665
#define High(number) ((int)number&0xFF00)>>8
#define Low(number) ((int)number&0x00FF)
#define HighLow(h,l) ((((int)h<<8)&0xff00)|((int)l&0xff))
const int bufferSize = 4096;
const unsigned char SHAKE_1 = 0x01;
const unsigned char SHAKE_2 = 0x02;
const unsigned char SHAKE_3 = 0x04;
const unsigned char WAVE_1 = 0x80;
const unsigned char WAVE_2 = 0x40;
char buffer[200000000];
int len;


struct MESSAGE {
	int server_port;//�˿ں�
	int seq_num;//���
	int length;//���Ķζ����Ƴ���
	int check_sum;//У���
	char* message;//���Ķ�
	MESSAGE() {}
	//��������ݰ���ʽ��
	MESSAGE(int server_port, int seq_num, int length, int check_sum, char* message) :
		server_port(server_port),
		seq_num(seq_num),
		length(length),
		check_sum(check_sum),
		message(message) {}
	void print() {
		printf("�˿�:%d\n���ݰ����к�:%d\n���ݶγ���:%d\nУ���:%d\n����:%s\n",
			server_port, seq_num, length, check_sum, message);
	}
	//�����ı��ģ������ƴ������浽msg�� 
	void send_message(char* msg) {
		//��Ĳ���ÿһλ�����ǰ�8λ��Ϊһ���ֽڣ��浽char��һ����λ�
		msg[0] = High(server_port);
		msg[1] = Low(server_port);//ǰ��λ��˿ں�
		msg[2] = High(seq_num);
		msg[3] = Low(seq_num);//֮������к�
		msg[4] = High(length);
		msg[5] = Low(length);//֮�������ݳ���
		msg[6] = 0;
		msg[7] = 0;//У���
		for (int i = 0; i < length; ++i) {
			msg[8 + i] = message[i];//֮�󶼴�����
		}

		int a = checksum(msg);//��������֮���ټ���У���
		msg[6] = High(a);
		msg[7] = Low(a);
	}
	//����У��ͣ�ÿ16λתΪ10���ƣ�Ȼ�����ȡ��
	int checksum(char* msg) {
		unsigned long sum = 0;
		for (int i = 0; i < 8 + length; i += 2) {//���ﲻ����strlen(msg)����Ϊ����м���0�������Ͳ�������
			sum += HighLow(msg[i], msg[i + 1]);
			sum = (sum >> 16) + (sum & 0xffff);
			//��16λ�����Լ��Ľ�λ���֣��൱�ڻؾ�
			//�ö����ƾ�������Ϊ�����Ϊ111+111=1110=1111-1�����Լ��Ͻ�λ��϶������ٽ�λ�� 
		}
		return (~sum) & 0xffff;
	}
};

//����udp����msg�����ر��ĶΣ�д��message
bool handle(char* msg, char* message) {
	int server_port = HighLow(msg[0], msg[1]);
	int seq_num = HighLow(msg[2], msg[3]);
	int length = HighLow(msg[4], msg[5]);
	int check_sum = HighLow(msg[6], msg[7]);
	for (int i = 0; i < length; i++) {
		message[i] = msg[8 + i];
	}
	//	MESSAGE m=MESSAGE(server_port,seq_num,length,check_sum,message);
	//	m.print();
	//ֱ�Ӽ���У���
	int sum = 0;
	for (int i = 0; i < length + 8; i += 2) {
		sum += HighLow(msg[i], msg[i + 1]);
		sum = (sum >> 16) + (sum & 0xffff);
	}
	if (sum == 0xffff) {//У�����ȷ���ظ�ACK
		return true;
	}
	else {
		return false;
	}
}

double cwnd = 1.0;	//���ڴ�С
double ssthresh = 8.0;	//��ֵ��һ���ﵽ��ֵ����ָ��->����
int dup_ack_cnt = 0;//����ack������
int last_ack_seq = 0;//��һ�ε�ack��ţ����ڸ���ack_cnt

SOCKET localSocket;
struct sockaddr_in serverAddr, clientAddr;//���ն˵�ip�Ͷ˿ں���Ϣ 

DWORD WINAPI handlerRequest(LPVOID lpParam);//������յ��߳�

bool begin_recv = false;//���Կ�ʼ���� 
bool waiting = false; //�ȴ����� 
string temp;

int nextseqnum = 0;//��ţ�����nxtseqnum


//���ͱ���
void send_to(char* message) {
	MESSAGE u = MESSAGE(SERVER_PORT, nextseqnum, 1024 - 10, 0, message);//�����udp 
	//�����ı���msg
	char msg[bufferSize];
	u.send_message(msg);
	//��������
	sendto(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));//��С����Ҫ
}


//���� 
void recv_from(char* message) {
	char msg[bufferSize];
	int size = sizeof(serverAddr);
	recvfrom(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&serverAddr, &size);
	handle(msg, message);
}


//�����ļ�����У���
void send_file(string path) {
	FILE* fin = fopen(path.c_str(), "rb");
	char buffer[4096];
	while (!feof(fin))
	{
		fread(buffer, 1, sizeof(buffer), fin);
		cout << strlen(buffer) << endl;
		sendto(localSocket, buffer, sizeof(buffer), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
		memset(buffer, 0, sizeof(buffer));
	}
	cout << "���" << endl;
	fclose(fin);
}


double dt;
clock_t start, end;
double TIME_OUT = 10000;//��ʱʱ�� 
//���ļ��ֿ�洢��v��
vector<char*>v;//��Ƭ����
char block[12000][1024];


clock_t all_start, all_end;
int base = 0;
void send_file_2(string path) {
	//�ȱ��浽������
	FILE* fin = fopen(path.c_str(), "rb");//�ȶ��ļ�
	int i = 0;
	while (!feof(fin)) {
		fread(block[i], 1, sizeof(block[i]) - 10, fin);
		v.push_back(block[i++]);//��������
	}
	fclose(fin);

	char* message;
	int n = v.size();

	all_start = clock();
	while (nextseqnum < n)
	{
		int end = clock();
		dt = (double)(end - start);
		//�����ʱ��δȷ�ϵ�ȫ���ش�
		if (dt >= TIME_OUT) {
			cout << "��ʱ����ʱʱ��Ϊ��" << dt << endl;
			ssthresh =cwnd/ 2;
			cwnd = 1;
			dup_ack_cnt = 0;
			nextseqnum = base;//�ط������˵�base
			cout << "��ʱ�����ڱ仯Ϊ��" << cwnd << endl;
			cout << "��ʱ����ֵ�仯Ϊ��" << ssthresh << endl;
		}
		//�������пռ䣬�Ҳ���ͣ��״̬ʱ�������ͱ��� 
		if (nextseqnum < base + cwnd)
		{
			message = v[nextseqnum];
			send_to(message);
			start = clock();
			begin_recv = true;//***���Կ�ʼ���շ���������Ϣ��***
			//cout<<nextseqnum<<' '<<strlen(message)<<endl;
			nextseqnum++; //�µı��ģ���ű仯
		}
	}
	all_end = clock();
	cout << "����" << endl;
}
//double space = 1859130;
//void send_test() {
//	send_file_2("E:\\1.jpg");
//}

int main() {
	//��ʼ��
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 1);
	int error=WSAStartup(wVersionRequested, &wsaData);
	if (error) {
		printf("init error");
		return 0;
	}
	cout << "������Ŀ�Ķ˿ں�(·����4001��������6666)��";
	int sendbuf;
	cin >> sendbuf;
	//ȥ���ӷ�������socket
	localSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	//��������ip�Ͷ˿ں� 
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(sendbuf);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(6665);
	clientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	HANDLE hThread = CreateThread(NULL, 0, handlerRequest, LPVOID(), 0, NULL);

	//cout << "������Ŀ�Ķ˿ں�(·����4001��������6666)��";
	
	//cin >> sendbuf;
	string filename;
	while (1) {
		printf("������Ҫ���͵��ļ�����");
		cin >> filename;
		ifstream fin(filename.c_str(), ifstream::binary);
		if (!fin) {
			printf("�ļ�δ�ҵ�!\n");
			continue;
		}
		else {
			cout << "�ҵ��ļ�����ʼ����..." << endl;
			unsigned char t = fin.get();
			while (fin) {
				buffer[len++] = t;
				t = fin.get();
			}
			fin.close();
			send_file_2(filename);
		}
		
		break;
	}
	//string sendbuf;
	//cin >> sendbuf;
	//send_test();
	double all_pass = (double)(all_end - all_start) / CLOCKS_PER_SEC;
	

	Sleep(10000);//һ��Ҫ��һ�ᣬ�����߳������ص���û����ӡ��� 
	CloseHandle(hThread);
	closesocket(localSocket);
	WSACleanup();
	cout << "����ʱ��Ϊ��" << all_pass << "s" << endl;
	cout << "ƽ��������Ϊ��" << len*8 /1000/ all_pass << "kbps" << endl;
	return 0;
}


//������յ��߳� 
DWORD WINAPI handlerRequest(LPVOID lpParam) {
	while (1) {
		Sleep(10);//�ı�begin_recv����Ҫ��һ���ٽ��գ���Ȼ�ᶪ����һ��ACK 
		char msg[bufferSize], message[bufferSize];
		int size = sizeof(serverAddr);
		if (begin_recv == false) {
			continue;
		}
		recvfrom(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&serverAddr, &size);


		if (handle(msg, message)) {
			int new_ack_seq = HighLow(msg[2], msg[3]);
			cout << "ACK" << new_ack_seq << endl;
			base = new_ack_seq + 1;

			if (new_ack_seq != last_ack_seq) {//ack��Ų�����
				last_ack_seq = new_ack_seq;
				dup_ack_cnt = 0;
				//ӵ������״̬
				if (cwnd >= ssthresh) {
					cwnd += 1 / cwnd;
					cout << "����ӵ�����⣬���ڱ仯Ϊ��" << cwnd << endl;
					cout << "����ӵ�����⣬��ֵΪ��" << ssthresh << endl;
				}
				//������״̬��ÿ�յ�һ��ack������+1��һ�ֹ������*2
				else {
					cwnd++;
					cout << "������״̬������Ϊ��" << cwnd << endl;
					cout << "������״̬����ֵΪ��" << ssthresh << endl;
				}
			}

			else {//����
				dup_ack_cnt++;
				if (dup_ack_cnt == 3) {//3������ack������ֵ=cwnd/2,cwnd=��ֵ+3
					ssthresh = cwnd / 2;
					cwnd = ssthresh + 3;
					nextseqnum = base;//�ش������˵�base
					cout << "3���ظ�ack�����ڱ仯Ϊ��" << cwnd << endl;
					cout << "3���ظ�ack����ֵΪ��" << ssthresh << endl;
				}
			}
			//���յ�һ��ack������һ������ʼ��ʱ
			//cout << "���ڴ�С��"<<cwnd << endl;
			start = clock();
		}
	}
}