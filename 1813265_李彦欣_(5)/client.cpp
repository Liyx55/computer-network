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
#define SERVER_PORT 4001//路由器转发的端口
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
		//存的不是每一位，而是把8位作为一个字节，存到char的一个单位里！
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

double cwnd = 1.0;	//窗口大小
double ssthresh = 8.0;	//阈值，一旦达到阈值，则指数->线性
int dup_ack_cnt = 0;//冗余ack计数器
int last_ack_seq = 0;//上一次的ack序号，用于更新ack_cnt

SOCKET localSocket;
struct sockaddr_in serverAddr, clientAddr;//接收端的ip和端口号信息 

DWORD WINAPI handlerRequest(LPVOID lpParam);//负责接收的线程

bool begin_recv = false;//可以开始接收 
bool waiting = false; //等待发送 
string temp;

int nextseqnum = 0;//序号，就是nxtseqnum


//发送报文
void send_to(char* message) {
	MESSAGE u = MESSAGE(SERVER_PORT, nextseqnum, 1024 - 10, 0, message);//打包成udp 
	//打包后的报文msg
	char msg[bufferSize];
	u.send_message(msg);
	//发送数据
	sendto(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));//大小很重要
}


//接收 
void recv_from(char* message) {
	char msg[bufferSize];
	int size = sizeof(serverAddr);
	recvfrom(localSocket, msg, sizeof(msg), 0, (SOCKADDR*)&serverAddr, &size);
	handle(msg, message);
}


//发送文件，无校验和
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
	cout << "完成" << endl;
	fclose(fin);
}


double dt;
clock_t start, end;
double TIME_OUT = 10000;//超时时间 
//把文件分块存储在v中
vector<char*>v;//分片发送
char block[12000][1024];


clock_t all_start, all_end;
int base = 0;
void send_file_2(string path) {
	//先保存到数组里
	FILE* fin = fopen(path.c_str(), "rb");//先读文件
	int i = 0;
	while (!feof(fin)) {
		fread(block[i], 1, sizeof(block[i]) - 10, fin);
		v.push_back(block[i++]);//加入数组
	}
	fclose(fin);

	char* message;
	int n = v.size();

	all_start = clock();
	while (nextseqnum < n)
	{
		int end = clock();
		dt = (double)(end - start);
		//如果超时，未确认的全部重传
		if (dt >= TIME_OUT) {
			cout << "超时，超时时间为：" << dt << endl;
			ssthresh =cwnd/ 2;
			cwnd = 1;
			dup_ack_cnt = 0;
			nextseqnum = base;//重发，回退到base
			cout << "超时，窗口变化为：" << cwnd << endl;
			cout << "超时，阈值变化为：" << ssthresh << endl;
		}
		//当现在有空间，且不在停等状态时，允许发送报文 
		if (nextseqnum < base + cwnd)
		{
			message = v[nextseqnum];
			send_to(message);
			start = clock();
			begin_recv = true;//***可以开始接收服务器端消息啦***
			//cout<<nextseqnum<<' '<<strlen(message)<<endl;
			nextseqnum++; //新的报文，序号变化
		}
	}
	all_end = clock();
	cout << "结束" << endl;
}
//double space = 1859130;
//void send_test() {
//	send_file_2("E:\\1.jpg");
//}

int main() {
	//初始化
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 1);
	int error=WSAStartup(wVersionRequested, &wsaData);
	if (error) {
		printf("init error");
		return 0;
	}
	cout << "请输入目的端口号(路由器4001，服务器6666)：";
	int sendbuf;
	cin >> sendbuf;
	//去连接服务器的socket
	localSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	//服务器的ip和端口号 
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(sendbuf);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(6665);
	clientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	HANDLE hThread = CreateThread(NULL, 0, handlerRequest, LPVOID(), 0, NULL);

	//cout << "请输入目的端口号(路由器4001，服务器6666)：";
	
	//cin >> sendbuf;
	string filename;
	while (1) {
		printf("请输入要发送的文件名：");
		cin >> filename;
		ifstream fin(filename.c_str(), ifstream::binary);
		if (!fin) {
			printf("文件未找到!\n");
			continue;
		}
		else {
			cout << "找到文件，开始发送..." << endl;
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
	

	Sleep(10000);//一定要等一会，否则线程立即关掉，没法打印结果 
	CloseHandle(hThread);
	closesocket(localSocket);
	WSACleanup();
	cout << "传输时间为：" << all_pass << "s" << endl;
	cout << "平均吞吐率为：" << len*8 /1000/ all_pass << "kbps" << endl;
	return 0;
}


//负责接收的线程 
DWORD WINAPI handlerRequest(LPVOID lpParam) {
	while (1) {
		Sleep(10);//改变begin_recv后，需要等一下再接收，不然会丢掉第一个ACK 
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

			if (new_ack_seq != last_ack_seq) {//ack序号不冗余
				last_ack_seq = new_ack_seq;
				dup_ack_cnt = 0;
				//拥塞避免状态
				if (cwnd >= ssthresh) {
					cwnd += 1 / cwnd;
					cout << "发生拥塞避免，窗口变化为：" << cwnd << endl;
					cout << "发生拥塞避免，阈值为：" << ssthresh << endl;
				}
				//慢启动状态，每收到一个ack，窗口+1，一轮过后就是*2
				else {
					cwnd++;
					cout << "慢启动状态，窗口为：" << cwnd << endl;
					cout << "慢启动状态，阈值为：" << ssthresh << endl;
				}
			}

			else {//冗余
				dup_ack_cnt++;
				if (dup_ack_cnt == 3) {//3个冗余ack，则阈值=cwnd/2,cwnd=阈值+3
					ssthresh = cwnd / 2;
					cwnd = ssthresh + 3;
					nextseqnum = base;//重传，回退到base
					cout << "3个重复ack，窗口变化为：" << cwnd << endl;
					cout << "3个重复ack，阈值为：" << ssthresh << endl;
				}
			}
			//当收到一个ack，则下一个包开始计时
			//cout << "窗口大小："<<cwnd << endl;
			start = clock();
		}
	}
}