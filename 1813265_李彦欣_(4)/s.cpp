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
const int TIMEOUT = 5000;//����
char buffer[200000000];
int len;

SOCKADDR_IN serverAddr, clientAddr;
SOCKET server; //ѡ��udpЭ��


unsigned char cksum(char* flag, int len) {//����У���
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
void wait_shake_hand() {//�ȴ���������
    while (1) {
        char recv[2];
        int connect = 0;
        int lentmp = sizeof(clientAddr);
        while (recvfrom(server, recv, 2, 0, (sockaddr*)&clientAddr, &lentmp) == SOCKET_ERROR);
        if (cksum(recv, 2) != 0 || recv[1] != SHAKE_1)//������֮�����У��Ͳ�����0���߽��յ��Ĳ���shake1
            continue;

        while (1) {
            recv[1] = SHAKE_2;
            recv[0] = cksum(recv + 1, 1);
            sendto(server, recv, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));//���͵ڶ�������
            while (recvfrom(server, recv, 2, 0, (sockaddr*)&clientAddr, &lentmp) == SOCKET_ERROR);
            if (cksum(recv, 2) == 0 && recv[1] == SHAKE_1)
                continue;//���ж��Ƿ�У���Ϊ0���Ƿ���ܵ���ȷ��shake
            if (cksum(recv, 2) != 0 || recv[1] != SHAKE_3) {
                printf("���ӽ���ʧ�ܣ��������ͻ��ˡ�");
                connect = 1;
            }
            break;
        }
        if (connect == 1)
            continue;
        break;
    }
}

void wait_wave_hand() {//�ȴ�����
    while (1) {
        char recv[2];
        int lentmp = sizeof(clientAddr);
        while (recvfrom(server, recv, 2, 0, (sockaddr*)&clientAddr, &lentmp) == SOCKET_ERROR);//��ʼ���ջ�����Ϣ
        if (cksum(recv, 2) != 0 || recv[1] != (char)WAVE_1)//У��Ͳ�����0��δ���յ�wave1
            continue;
        recv[1] = WAVE_2;
        recv[0] = cksum(recv + 1, 1);
        sendto(server, recv, 2, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));//����wave2
        break;
    }
}

void recv_message(char* message, int& len_recv) {
    char recv[Mlenx + 4];
    int lentmp = sizeof(clientAddr);
    static unsigned char last_order = 0;
    len_recv = 0;//��¼�ļ����� 
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


            send[1] = ACK;//��־λ
            send[2] = last_order - 1;//���к�
            send[0] = cksum(send + 1, 2);//У���
            sendto(server, send, 3, 0, (sockaddr*)&clientAddr, sizeof(clientAddr));//����ACK��ȥ
            if (flag)
                break;
        }

        if (LAST_PACK == recv[1]) {//���һ��������һ������λ
            if (recv[3] + 4 < 4 && recv[3] + 4 > -127) {//���ȳ���127�������Ǹ�ֵ����Ҫ����256��Ϊ��
                for (int i = 4; i < (int)recv[3] + 4 + 256; i++) {
                    message[len_recv++] = recv[i];//˳����յ�����   

                }
            }
            else if (recv[3] + 4 < -127) {
                for (int i = 4; i < (int)recv[3] + 4 + 256 + 128; i++) {
                    message[len_recv++] = recv[i];//˳����յ�����   

                }
            }
            else {
                for (int i = 4; i < (int)recv[3] + 4; i++) {
                    message[len_recv++] = recv[i];//˳����յ�����   

                }
            }
            //printf("��󣡽��յ�bytesΪ�� %d\n", len_recv);
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

    serverAddr.sin_family = AF_INET; //ʹ��ipv4
    serverAddr.sin_port = htons(Port); //�˿�
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    server = socket(AF_INET, SOCK_DGRAM, 0);
    //���÷�����
    int time_out = 1;//1ms��ʱ
    //����û��������ݷ��ͳ�ȥ���ڹر�socket��������1ms
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

    //�ȴ�����
    printf("�ȴ�����...\n");
    wait_shake_hand();
    printf("�û��ѽ��롣\n���ڽ�������...\n");
    recv_message(buffer, len);
    printf("��һ����Ϣ���ճɹ���");
    buffer[len] = 0;
    cout << buffer << endl;
    string file_name("hello");
    clock_t start, end, time;
    printf("��ʼ���յڶ�����Ϣ...\n");
    start = clock();
    recv_message(buffer, len);
    end = clock();
    time = (end - start) / CLOCKS_PER_SEC;
    printf("�ڶ�����Ϣ���ճɹ���\n");
    ofstream fout(file_name.c_str(), ofstream::binary);
    for (int i = 0; i < len; i++)
        fout << buffer[i];
    fout.close();
    printf("���յ����ļ�bytes:%d\n", len);
    cout << "����ʱ��Ϊ��" << (double)time << "s" << endl;
    cout << "ƽ��������Ϊ��" << len*8 / 1000 / (double)time << "kbps" << endl;
    wait_wave_hand();
    printf("���ӶϿ�");
    return 0;
}