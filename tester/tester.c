//this is a client program to test the server
#include <stdio.h>
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <math.h>

#define numberC 30

#pragma comment(lib, "ws2_32.lib")

SOCKET client_sock[numberC];
HANDLE hThread[numberC], cThread;
int numberofTests[numberC] = { 0 };
unsigned int tID, cID;

char ipAport[30] = { 0 };
char ip[15] = { 0 };
char portC[10] = { 0 };
SOCKADDR_IN server;

const char httpR[] = "GET / HTTP/1.1";

unsigned int __stdcall count(void *arglist)
{
    int total = 0, last_total = 0, sec = 0;
    while(TRUE){
        last_total = total;
        total = 0;
        Sleep(1000);
        sec++;
        for(int i = 0; i < numberC; i++){
            total += numberofTests[i];
        }
        printf("After %d sec: the speed is %d tests/sec\n", sec, (total - last_total));
    }
}

unsigned int __stdcall process(void *arglist)
{
    int curThread = (int)arglist;

    char buf[2048];
    int i = 0;
    while(TRUE){
        //client_sock[curThread] = socket(AF_INET, SOCK_STREAM, 0);
        client_sock[curThread] = socket(AF_INET, SOCK_STREAM, 0);
        if(connect(client_sock[curThread], (struct sockaddr *)&server, sizeof(server)) == 0){
            send(client_sock[curThread], httpR, strlen(httpR), 0);
            while(recv(client_sock[curThread], buf, 2048, 0) > 0){
                //printf("%s", buf);
                //printf("%d: %d\n", curThread, i);
                numberofTests[curThread]++;
            }
            closesocket(client_sock[curThread]);
        }
        else{
            printf("Err: %d\n", WSAGetLastError());
        }
        Sleep(100);
        i++;
    }
    _endthreadex(0);
}

int charToInt(char *portC){
    int length = strlen(portC);
    int port = 0;

    int number = 0;
    for(int i = 0; i < length; i++){
        number = portC[i] - 48;
        port += number * pow(10, (length - i - 1));
    }
    return port;
}

int main()
{
    WSADATA wsa;

    if(WSAStartup(MAKEWORD(1, 1), &wsa) != 0)
	{
		return 1;
	}

    int i;

    scanf_s("%s", ipAport, 30);
    for(i = 0; ipAport[i] != ':'; i++){
        ip[i] = ipAport[i];
    }

    i++;
    int j;
    for(j = 0; i < strlen(ipAport); i++, j++){
        portC[j] = ipAport[i];
    }

    int port = charToInt(portC);

    ZeroMemory(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    for(int i = 0; i < numberC; i++){
        hThread[i] = (HANDLE)_beginthreadex(NULL, 0, process, (void *)i, 0, &tID);
    }
    cThread = (HANDLE)_beginthreadex(NULL, 0, count, NULL, 0, &cID);
    while(TRUE){

    }
    
    for(int i = 0; i < numberC; i++){
        closesocket(client_sock[i]);
        CloseHandle(hThread[i]);
    }
    CloseHandle(cThread);
    WSACleanup();
    return 0;
}