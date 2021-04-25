#pragma comment(lib, "Ws2_32.lib")

#include <iostream>
#include <winsock2.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <string>
#include <time.h>
#include <queue>

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
int WINDOW_SIZE;
const int TIMEOUT = 5000;//����
char buffer[200000000];
int len;

SOCKET client;
SOCKADDR_IN serverAddr, clientAddr;

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
    //cout << ~(ret & 0x00FF);
    return ~(ret & 0x00FF);
}

void wave_hand() {//���л��֣��Ͽ�����
    int tot_fail = 0;//��¼ʧ�ܴ���
    while (1) {
        //����wave_1
        char tmp[2];//�������ݻ�����
        tmp[1] = WAVE_1;//�ڶ�λ��¼���ֵ��ֶ�
        tmp[0] = cksum(tmp + 1, 1);//��һλ��¼У���
        //cout << "wave1У���Ϊ��" << int(tmp[0]);
        sendto(client, tmp, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));//���͵���������
        int begintime = clock();
        char recv[2];
        int lentmp = sizeof(serverAddr);
        int fail_send = 0;//��ʶ�Ƿ���ʧ��
        while (recvfrom(client, recv, 2, 0, (sockaddr*)&serverAddr, &lentmp) == SOCKET_ERROR)
            if (clock() - begintime > TIMEOUT) {//�����Ѿ���ʱ
                fail_send = 1;//����ʧ��
                tot_fail++;//ʧ�ܴ�����һ
                break;
            }
        //cout << tot_fail<<endl;
        //cout << fail_send<<endl;
        //����wave_2��У��
        if (fail_send == 0 && cksum(recv, 2) == 0 && recv[1] == WAVE_2)
            break;
        else {//δ�յ�
            
            if (tot_fail == 3) {
                printf("����ʧ��...");
                break;
            }
            continue;
        }
    }
}


bool send_package(char* message, int lent, int order, int last = 0) {//��Ƭ����
    if (lent > Mlenx) {//��Ƭ���ȹ���
        return false;
    }
    if (last == false && lent != Mlenx) {//�������һ����Ҳ���ǵ���Ƭ����
        return false;
    }
    char* tmp;
    int tmp_len;

    if (last) {//�����һ����
        tmp = new char[lent + 4];//���仺����
        tmp[1] = LAST_PACK;
        tmp[2] = order;//���к�
        tmp[3] = lent;//��һ�����ȵĴ洢
        for (int i = 4; i < lent + 4; i++)
            tmp[i] = message[i - 4];//��������
        tmp[0] = cksum(tmp + 1, lent + 3);//��һλ����У���
        //cout << "lastbaoУ���Ϊ��" << int(tmp[0]);
        tmp_len = lent + 4;//���Ǽ�¼һ�³���
    }
    else {
        tmp = new char[lent + 3];
        tmp[1] = NOTLAST_PACK;//�������һ����
        tmp[2] = order;
        for (int i = 3; i < lent + 3; i++)
            tmp[i] = message[i - 3];
        tmp[0] = cksum(tmp + 1, lent + 2);
        //cout << "bushilastbaoУ���Ϊ��" << int(tmp[0]);
        tmp_len = lent + 3;
    }
    sendto(client, tmp, tmp_len, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));//����Ƭ����Ϣ���͹�ȥ
    return true;
}


void shake_hand() {//���֣���������
    while (1) {
        //����shake_1
        char tmp[2];
        tmp[1] = SHAKE_1;
        tmp[0] = cksum(tmp + 1, 1);
        //cout << "shake1У���Ϊ��" << int(tmp[0]);
        sendto(client, tmp, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
        int begintime = clock();
        char recv[2];
        int lentmp = sizeof(clientAddr);
        int fail_send = 0;
        while (recvfrom(client, recv, 2, 0, (sockaddr*)&serverAddr, &lentmp) == SOCKET_ERROR)
            if (clock() - begintime > TIMEOUT) {//�ѳ�ʱ
                fail_send = 1;
                break;
            }
        //����shake_2��У��
        if (fail_send == 0 && cksum(recv, 2) == 0 && recv[1] == SHAKE_2) {
            {
                //����shake_3
                tmp[1] = SHAKE_3;
                tmp[0] = cksum(tmp + 1, 1);
                //cout << "shake2У���Ϊ��" << int(tmp[0]);
                sendto(client, tmp, 2, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
                break;
            }
        }
    }
}

bool in_list[UCHAR_MAX + 1];//���кű��256
void send_message(char* message, int lent) {//lab3-2
    queue<pair<int, int>> timer_list;//����timer��¼���ͳ�ȥ��ʱ���, order
    int leave_cnt = 0;
    static int base = 0;
    int has_send = 0;//�ѷ���δȷ��
    int nextseqnum = base;
    int has_send_succ = 0;//��ȷ��
    int tot_package = lent / Mlenx + (lent % Mlenx != 0);//ȷ�����������Ƿ�����һ����
    while (1) {
        if (has_send_succ == tot_package)//���ж��Ƿ���ȫ�����ͳɹ�
            break;
        //�ڷ��ʹ���δ����ʱ���߷��ͱ߽��н��ռ�⣬�鿴�Է����Ƿ����ۼ�ȷ��״̬�뷢�͹�����
        if (timer_list.size() < WINDOW_SIZE && has_send != tot_package) {
            send_package(message + has_send * Mlenx,
                has_send == tot_package - 1 ? lent - (tot_package - 1) * Mlenx : Mlenx,//�����һ��������ȡǰ���Ǹ�
                nextseqnum % ((int)UCHAR_MAX + 1),//���к�
                has_send == tot_package - 1);//�Ƿ������һ����
            timer_list.push(make_pair(clock(), nextseqnum % ((int)UCHAR_MAX + 1)));//��ʼ��ʱ����ĩβ����һ����Ԫ��
            in_list[nextseqnum % ((int)UCHAR_MAX + 1)] = 1;//����¼���кţ����봰�ڵļ�Ϊ1
            nextseqnum++;
            has_send++;
        }
        //ʹ����whileѭ�����ص���recv��send����ʵ�֣�������ʹ���̵߳��µķ�����
        //һ���ۼ�ȷ��״̬����Ÿ��ڵ�ǰ��base��ŵİ�������з��ʹ��ڵĻ�����
        //���Ҹ��¶�ʱ�����������ݰ����͡�
        char recv[3];
        int lentmp = sizeof(serverAddr);
        //�������˾͵ȴ�ackȷ�Ϻ󴰿ڼ�С�����ܼ�������
        if (recvfrom(client, recv, 3, 0, (sockaddr*)&serverAddr, &lentmp) != SOCKET_ERROR && cksum(recv, 3) == 0 &&
            recv[1] == ACK && in_list[(unsigned char)recv[2]]) {//recv2�����кţ�����ACK
            //�ۼ�ȷ��
            while (timer_list.front().second != (unsigned char)recv[2]) {//�����е�һ��Ԫ�ص����кŲ����ڵ�ǰ���кŵĻ�
                has_send_succ++;//��ȷ��
                base++;//�����ƶ�
                in_list[timer_list.front().second] = 0;//�����һ��Ԫ�ص����кţ����ڴ��ڣ���Ϊ0
                timer_list.pop();//ɾ����һ��Ԫ��
            }
            //δ��ʱ�յ���������Ҫ�������ж��ף������¼�ʱ����
            in_list[timer_list.front().second] = 0;//��Ӧ�����кţ����ڴ����ڣ���Ϊ0
            has_send_succ++;
            base++;
            leave_cnt = 0;
            timer_list.pop();//ɾ����һ��Ԫ��
        }
        else {
            //��ʱ����л����ط���������ն���
            if (clock() - timer_list.front().first > TIMEOUT) {//��ǰclock()�ж϶��׵İ��Ƿ��ͳ�ʱ
                nextseqnum = base;//���˵�base
                leave_cnt++;
                has_send -= timer_list.size();//��ȥ���ڶ����е�Ԫ�ظ�������С���ڵȴ������ݰ�
                //δ��ʱ�յ���������Ҫ�������ж��ף������¼�ʱ����
                while (!timer_list.empty()) //��Ϊ��
                    timer_list.pop();//ɾ����һ��Ԫ��
            }
        }
        //cout << leave_cnt;

        //if (leave_cnt >= 5) {
        //    wave_hand();//�Ѿ�����ʹ���5��
        //    return;
        //}
        if (base % 100 == 0)
            printf("���ļ��Ѿ�����%.2f%%\n", (float)base / tot_package * 100);
    }
}


int main() {


    WSADATA wsadata;
    int error = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (error) {
        printf("init error");
        return 0;
    }
    string serverip;
    while (1) {
        printf("��������շ�ip��ַ:\n");
        getline(cin, serverip);

        if (inet_addr(serverip.c_str()) == INADDR_NONE) {
            printf("ip��ַ���Ϸ�!\n");
            continue;
        }
        break;
    }

    int port = 11451;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(serverip.c_str());
    client = socket(AF_INET, SOCK_DGRAM, 0);
    //���÷�����
    int time_out = 1;//1ms��ʱ
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));

    if (client == INVALID_SOCKET) {
        printf("creat udp socket error");
        return 0;
    }
    string filename;
    while (1) {
        printf("������Ҫ���͵��ļ�����");
        cin >> filename;
        ifstream fin(filename.c_str(), ifstream::binary);
        if (!fin) {
            printf("�ļ�δ�ҵ�!\n");
            continue;
        }
        unsigned char t = fin.get();
        while (fin) {
            buffer[len++] = t;
            t = fin.get();
        }
        fin.close();
        break;
    }
    printf("�����뷢�ʹ��ڴ�С��\n");
    cin >> WINDOW_SIZE;
    WINDOW_SIZE %= UCHAR_MAX; //��ֹ���ڴ�С��������򳤶�
    printf("���ӽ�����...\n");
    shake_hand();
    printf("���ӽ�����ɡ� \n���ڷ�����Ϣ...\n");
    clock_t start, end,time;
    send_message((char*)(filename.c_str()), filename.length());
    printf("�ļ���������ϣ����ڷ����ļ�����...\n");
    start = clock();
    send_message(buffer, len);
    end = clock();
    time = (end - start) / CLOCKS_PER_SEC;
    printf("���͵��ļ�bytes:%d\n", len);
    cout << "����ʱ��Ϊ��" << (double)time << "s" << endl;
    cout << "ƽ��������Ϊ��" << len*8 / 1000 / (double)time << "kbps" << endl;
    printf("�ļ����ݷ�����ϡ�\n ��ʼ�Ͽ�����...\n");
    wave_hand();
    printf("�����ѶϿ���");
    closesocket(client);
    WSACleanup();

    return 0;
}