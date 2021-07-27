//this is a client program to test the server
#include <stdio.h>
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <math.h>


#pragma comment(lib, "ws2_32.lib")

SOCKET client_sock[2048];
HANDLE hThread[2048];
unsigned int tID;

char ipAport[30] = { 0 };
char ip[15] = { 0 };
char portC[10] = { 0 };
SOCKADDR_IN server;


unsigned int __stdcall process(void *arglist)
{
    int curThread = (int)arglist;

    char buf[2048];
    while(TRUE){
        if(connect(client_sock[curThread], (struct sockaddr *)&server, sizeof(server)) == 0){
            if(recv(client_sock[curThread], buf, 2048, 0) > 0){
                printf("%s\n", buf);
            }
            else{
                
            }
        }
        closesocket(client_sock[curThread]);
    }
    Sleep(2000);
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

    for(int i = 0; i < 2048; i++){
        client_sock[i] = socket(AF_INET, SOCK_STREAM, 0);
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

    //printf("hh");
    for(int i = 0; i < 2048; i++){
        hThread[i] = (HANDLE)_beginthreadex(NULL, 0, process, (void *)i, 0, &tID);
        Sleep(500);
    }

    for(int i = 0; i < 2048; i++){
        closesocket(client_sock[i]);
        CloseHandle(hThread[i]);
    }
    WSACleanup();
    return 0;
}