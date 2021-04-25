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
//发送的ip和端口号信息 
struct sockaddr_in clientAddr, serverAddr;

#define High(number) ((int)number&0xFF00)>>8//得到高八位
#define Low(number) ((int)number&0x00FF)//得到低八位
#define HighLow(h,l) ((((int)h<<8)&0xff00)|((int)l&0xff))//得到全部
const int bufferSize = 4096;
const unsigned char SHAKE_1 = 0x01;
const unsigned char SHAKE_2 = 0x02;
const unsigned char SHAKE_3 = 0x04;
const unsigned char WAVE_1 = 0x80;
const unsigned char WAVE_2 = 0x40;


struct MESSAGE {
	int server_port;//端口号
	int seq_num;//序号
	int length;//报文段二进制长度
	int check_sum;//校验和
	char* message;//报文段
	MESSAGE() {}
	//定义的数据包格式：
	MESSAGE(int server_port, int seq_num, int length, int check_sum, char* message) :
		server_port(server_port),
		seq_num(seq_num),
		length(length),
		check_sum(check_sum),
		message(message) {}
	void print() {
		printf("端口:%d\n数据包序列号:%d\n数据段长度:%d\n校验和:%d\n数据:%s\n",
			server_port, seq_num, length, check_sum, message);
	}
	//真正的报文，二进制串，保存到msg中 
	void send_message(char* msg) {
		//存的不是每一位，而是把8位作为一个字节，存到char的一个单位里
		msg[0] = High(server_port);
		msg[1] = Low(server_port);//前两位存端口号
		msg[2] = High(seq_num);
		msg[3] = Low(seq_num);//之后存序列号
		msg[4] = High(length);
		msg[5] = Low(length);//之后是数据长度
		msg[6] = 0;
		msg[7] = 0;//校验和
		for (int i = 0; i < length; ++i) {
			msg[8 + i] = message[i];//之后都存数据
		}

		int a = checksum(msg);//存入数据之后再计算校验和
		msg[6] = High(a);
		msg[7] = Low(a);
	}
	//计算校验和，每16位转为10进制，然后求和取反
	int checksum(char* msg) {
		unsigned long sum = 0;
		for (int i = 0; i < 8 + length; i += 2) {//这里不能用strlen(msg)，因为如果中间有0，读到就不计算了
			sum += HighLow(msg[i], msg[i + 1]);
			sum = (sum >> 16) + (sum & 0xffff);
			//后16位加上自己的进位部分，相当于回卷
			//用二进制举例，因为和最大为111+111=1110=1111-1，所以加上进位后肯定不会再进位了 
		}
		return (~sum) & 0xffff;
	}
};

//解析udp报文msg，返回报文段，写入message
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
	//直接计算校验和
	int sum = 0;
	for (int i = 0; i < length + 8; i += 2) {
		sum += HighLow(msg[i], msg[i + 1]);
		sum = (sum >> 16) + (sum & 0xffff);
	}
	if (sum == 0xffff) {//校验和正确，回复ACK
		return true;
	}
	else {
		return false;
	}
}
//%1的概率比特错误或者丢包 
//bool random_loss() {
//	int r = rand() % 100;
//	cout << r << endl;
//	if (r > 98) {
//		return false;
//	}
//	return true;
//}


//发送报文
void send_to(char* message, int seq_num) {
	MESSAGE u = MESSAGE(SERVER_PORT, seq_num, strlen(message), 0, message);//打包成发送数据包的格式
	//打包后的报文msg
	char msg[bufferSize];
	u.send_message(msg);
	sendto(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&clientAddr, sizeof(SOCKADDR));
}

//收到并处理数据
bool recv_from() {
	char msg[bufferSize], message[bufferSize];
	int size = sizeof(clientAddr);
	recvfrom(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&clientAddr, &size);
	return handle(msg, message);
}

//无校验和接收文件
void recv_file(string path) {
	char buffer[bufferSize];

	FILE* fout = fopen(path.c_str(), "wb");//写入具体路径
	while (1)
	{
		int size = sizeof(clientAddr);
		recvfrom(localSocket, buffer, sizeof(buffer), 0, (SOCKADDR*)&clientAddr, &size);
		fwrite(buffer, 1, sizeof(buffer), fout);
		memset(buffer, 0, sizeof(buffer));
	}
	cout << "传输图片结束!" << endl;
	fclose(fout);
}

int expected_seqnum = 0;//希望接收的序列号
void recv_file_2(string path) {
	char msg[bufferSize], message[bufferSize];
	FILE* fout = fopen(path.c_str(), "wb");//将文件写入具体路径
	while (1)
	{
		int size = sizeof(clientAddr);
		recvfrom(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&clientAddr, &size);
		//bool error_msg = random_loss();
		//if (error_msg == false)msg[1] /= 2;

		bool check = handle(msg, message);

		//一定概率丢掉ACK包 
		//bool lost_ack = random_loss();
		//if (lost_ack == false)continue;

		int recv_seqnum = HighLow(msg[2], msg[3]);
		//如果校验和正确，并且收到的序号是想要的
		if (check == true && expected_seqnum == recv_seqnum) {
			cout << "接收成功" << endl;
			fwrite(message, 1, HighLow(msg[4], msg[5]), fout);
			send_to("ACK", expected_seqnum);
			expected_seqnum++;//更新想要的序号 
		}
		else {
			//否则发送最近正确接收的序号
			cout << "接收失败" << endl;
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

	//本地socket，只负责接收 
	localSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//服务器的ip和端口号 
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(6666);//htons把unsigned short类型从主机序转换到网络序
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	//绑定本地socket和端口号 
	bind(localSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
	cout << "服务器端口为：" << ntohs(serverAddr.sin_port) << endl;//ntohs把unsigned short类型从网络序转换到主机序
	printf("等待接入...\n");
	//wait_shake_hand();
	//printf("用户已接入。\n正在接收数据...\n");
	srand(time(0));
	recv_file_2("newmessage");
	closesocket(localSocket);
	WSACleanup();
	return 0;
}