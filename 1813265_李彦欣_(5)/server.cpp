#include<Winsock2.h> 
#pragma comment(lib,"ws2_32")
#include<stdio.h>
#include<string.h>
#include<iostream> 
#include<algorithm>
#include<time.h>
#include<cstdlib>
using namespace std;
#define SERVER_PORT 6666
#define CLIENT_PORT 6665
SOCKET localSocket;
//���͵�ip�Ͷ˿ں���Ϣ 
struct sockaddr_in clientAddr, serverAddr;

#define High(number) ((int)number&0xFF00)>>8//�õ��߰�λ
#define Low(number) ((int)number&0x00FF)//�õ��Ͱ�λ
#define HighLow(h,l) ((((int)h<<8)&0xff00)|((int)l&0xff))//�õ�ȫ��
const int bufferSize = 4096;
const unsigned char SHAKE_1 = 0x01;
const unsigned char SHAKE_2 = 0x02;
const unsigned char SHAKE_3 = 0x04;
const unsigned char WAVE_1 = 0x80;
const unsigned char WAVE_2 = 0x40;


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
		//��Ĳ���ÿһλ�����ǰ�8λ��Ϊһ���ֽڣ��浽char��һ����λ��
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
//%1�ĸ��ʱ��ش�����߶��� 
//bool random_loss() {
//	int r = rand() % 100;
//	cout << r << endl;
//	if (r > 98) {
//		return false;
//	}
//	return true;
//}


//���ͱ���
void send_to(char* message, int seq_num) {
	MESSAGE u = MESSAGE(SERVER_PORT, seq_num, strlen(message), 0, message);//����ɷ������ݰ��ĸ�ʽ
	//�����ı���msg
	char msg[bufferSize];
	u.send_message(msg);
	sendto(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&clientAddr, sizeof(SOCKADDR));
}

//�յ�����������
bool recv_from() {
	char msg[bufferSize], message[bufferSize];
	int size = sizeof(clientAddr);
	recvfrom(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&clientAddr, &size);
	return handle(msg, message);
}

//��У��ͽ����ļ�
void recv_file(string path) {
	char buffer[bufferSize];

	FILE* fout = fopen(path.c_str(), "wb");//д�����·��
	while (1)
	{
		int size = sizeof(clientAddr);
		recvfrom(localSocket, buffer, sizeof(buffer), 0, (SOCKADDR*)&clientAddr, &size);
		fwrite(buffer, 1, sizeof(buffer), fout);
		memset(buffer, 0, sizeof(buffer));
	}
	cout << "����ͼƬ����!" << endl;
	fclose(fout);
}

int expected_seqnum = 0;//ϣ�����յ����к�
void recv_file_2(string path) {
	char msg[bufferSize], message[bufferSize];
	FILE* fout = fopen(path.c_str(), "wb");//���ļ�д�����·��
	while (1)
	{
		int size = sizeof(clientAddr);
		recvfrom(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&clientAddr, &size);
		//bool error_msg = random_loss();
		//if (error_msg == false)msg[1] /= 2;

		bool check = handle(msg, message);

		//һ�����ʶ���ACK�� 
		//bool lost_ack = random_loss();
		//if (lost_ack == false)continue;

		int recv_seqnum = HighLow(msg[2], msg[3]);
		//���У�����ȷ�������յ����������Ҫ��
		if (check == true && expected_seqnum == recv_seqnum) {
			cout << "���ճɹ�" << endl;
			fwrite(message, 1, HighLow(msg[4], msg[5]), fout);
			send_to("ACK", expected_seqnum);
			expected_seqnum++;//������Ҫ����� 
		}
		else {
			//�����������ȷ���յ����
			cout << "����ʧ��" << endl;
			send_to("ACK", expected_seqnum - 1);
		}
		memset(message, 0, sizeof(message));
	}
	fclose(fout);
}

//void recv_test() {
//	recv_file_2("newmessage");
//}

int main() {
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 1);
	int nError = WSAStartup(wVersionRequested, &wsaData);
	if (nError) {
		printf("start error\n");
		return 0;
	}

	//����socket��ֻ������� 
	localSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//��������ip�Ͷ˿ں� 
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(6666);//htons��unsigned short���ʹ�������ת����������
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	//�󶨱���socket�Ͷ˿ں� 
	bind(localSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
	cout << "�������˿�Ϊ��" << ntohs(serverAddr.sin_port) << endl;//ntohs��unsigned short���ʹ�������ת����������
	printf("�ȴ�����...\n");
	//wait_shake_hand();
	//printf("�û��ѽ��롣\n���ڽ�������...\n");
	srand(time(0));
	recv_file_2("newmessage");
	closesocket(localSocket);
	WSACleanup();
	return 0;
}