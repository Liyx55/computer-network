#pragma comment(lib, "Ws2_32.lib")

#include <iostream>
#include <winsock2.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include<time.h>

using namespace std;

using namespace std;
const int Mlenx = 253;
const unsigned char ACK = 0x03;
const unsigned char NAK = 0x07;
const unsigned char LAST_PACK = 0x18;
const unsigned char NOTLAST_PACK = 0x08;
const unsigned char SHAKE_1 = 0x01;
const unsigned char SHAKE_2 = 0x02;
const unsigned char SHAKE_3 = 0x04;
const unsigned char WAVE_1 = 0x80;
const unsigned char WAVE_2 = 0x40;
const int TIMEOUT = 5000;//毫秒
char buffer[200000000];
int len;

SOCKADDR_IN serverAddr, clientAddr;
SOCKET server; //选择udp协议


unsigned char cksum(char* flag, int len) {//计算校验和
    if (len == 0) {
        return ~(0);
    }
    unsigned int ret = 0;
    int i = 0;
    while (len--) {
        ret += (unsigned char)flag[i++];
        if (ret & 0xFF00) {
            ret &= 0x00FF;
            ret++;
        }
    }
    return ~(ret & 0x00FF);
}
void wait_shake_hand() {//等待建立连接
    while (1) {
        char recv[2];
        int connect = 0;
        int lentmp = sizeof(clientAddr);
        while (recvfrom(server, recv, 2, 0, (sockaddr*)&clientAddr, &lentmp) == SOCKET_ERROR);
        if (cksum(recv, 2) != 0 || recv[1] != SHAKE_1)//接受了之后算出校验和不等于0或者接收到的不是shake1
            continue;

        while (1) {
            recv[1] = SHAKE_2;
            recv[0] = cksum(recv + 1, 1);
            sendto(server, recv, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));//发送第二次握手
            while (recvfrom(server, recv, 2, 0, (sockaddr*)&clientAddr, &lentmp) == SOCKET_ERROR);
            if (cksum(recv, 2) == 0 && recv[1] == SHAKE_1)
                continue;//先判断是否校验和为0和是否接受到正确的shake
            if (cksum(recv, 2) != 0 || recv[1] != SHAKE_3) {
                printf("链接建立失败，请重启客户端。");
                connect = 1;
            }
            break;
        }
        if (connect == 1)
            continue;
        break;
    }
}

void wait_wave_hand() {//等待挥手
    while (1) {
        char recv[2];
        int lentmp = sizeof(clientAddr);
        while (recvfrom(server, recv, 2, 0, (sockaddr*)&clientAddr, &lentmp) == SOCKET_ERROR);//开始接收挥手信息
        if (cksum(recv, 2) != 0 || recv[1] != (char)WAVE_1)//校验和不等于0或未接收到wave1
            continue;
        recv[1] = WAVE_2;
        recv[0] = cksum(recv + 1, 1);
        sendto(server, recv, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));//发送wave2
        break;
    }
}

void recv_message(char* message, int& len_recv) {
    char recv[Mlenx + 4];
    int lentmp = sizeof(clientAddr);
    static unsigned char last_order = 0;
    len_recv = 0;//记录文件长度 
    while (1) {
        while (1) {
            memset(recv, 0, sizeof(recv));
            while (recvfrom(server, recv, Mlenx + 4, 0, (sockaddr*)&clientAddr, &lentmp) == SOCKET_ERROR);
            char send[3];
            int flag = 0;
            if (cksum(recv, Mlenx + 4) == 0 && (unsigned char)recv[2] == last_order) {
                last_order++;
                flag = 1;
            }


            send[1] = ACK;//标志位
            send[2] = last_order - 1;//序列号
            send[0] = cksum(send + 1, 2);//校验和
            sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));//发送ACK回去
            if (flag)
                break;
        }

        if (LAST_PACK == recv[1]) {//最后一个包多了一个长度位
            if (recv[3] + 4 < 4 && recv[3] + 4 > -127) {//长度超过127，长度是负值，需要加上256变为正
                for (int i = 4; i < (int)recv[3] + 4 + 256; i++) {
                    message[len_recv++] = recv[i];//顺序接收到数据   

                }
            }
            else if (recv[3] + 4 < -127) {
                for (int i = 4; i < (int)recv[3] + 4 + 256 + 128; i++) {
                    message[len_recv++] = recv[i];//顺序接收到数据   

                }
            }
            else {
                for (int i = 4; i < (int)recv[3] + 4; i++) {
                    message[len_recv++] = recv[i];//顺序接收到数据   

                }
            }
            //printf("最后！接收的bytes为： %d\n", len_recv);
            break;
        }
        else {
            for (int i = 3; i < Mlenx + 3; i++)
                message[len_recv++] = recv[i];
        }
    }
}

int main() {


    WSADATA wsadata;
    int nError = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (nError) {
        printf("start error\n");
        return 0;
    }

    int Port = 11451;

    serverAddr.sin_family = AF_INET; //使用ipv4
    serverAddr.sin_port = htons(Port); //端口
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    server = socket(AF_INET, SOCK_DGRAM, 0);
    //设置非阻塞
    int time_out = 1;//1ms超时
    //即让没发完的数据发送出去后在关闭socket，允许逗留1ms
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));

    if (server == INVALID_SOCKET) {
        printf("create fail");
        closesocket(server);
        return 0;
    }
    int retVAL = bind(server, (sockaddr*)(&serverAddr), sizeof(serverAddr));
    if (retVAL == SOCKET_ERROR) {
        printf("bind fail");
        closesocket(server);
        WSACleanup();
        return 0;
    }

    //等待接入
    printf("等待接入...\n");
    wait_shake_hand();
    printf("用户已接入。\n正在接收数据...\n");
    recv_message(buffer, len);
    printf("第一条信息接收成功。");
    buffer[len] = 0;
    cout << buffer << endl;
    string file_name("hello");
    clock_t start, end, time;
    printf("开始接收第二条信息...\n");
    start = clock();
    recv_message(buffer, len);
    end = clock();
    time = (end - start) / CLOCKS_PER_SEC;
    printf("第二条信息接收成功。\n");
    ofstream fout(file_name.c_str(), ofstream::binary);
    for (int i = 0; i < len; i++)
        fout << buffer[i];
    fout.close();
    printf("接收到的文件bytes:%d\n", len);
    cout << "传输时间为：" << (double)time << "s" << endl;
    cout << "平均吞吐率为：" << len*8 / 1000 / (double)time << "kbps" << endl;
    wait_wave_hand();
    printf("链接断开");
    return 0;
}