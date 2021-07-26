#include <stdio.h>
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <Ws2tcpip.h>

#define number_connection 2049 //1 bigger than actual

struct connection
{
    SOCKET connec;
    HANDLE Thread;
    IN_ADDR ip;
    int port;
};

struct connection connections[number_connection - 1];
struct sockaddr_in fsin;
SOCKET ssock;
int numTh = 0;
//SOCKET connecs[2048];
//HANDLE connecThread[2048] = { NULL };

int deleteClient(int curThread)
{
    //printf("%d\n", connections[curThread].Thread);
    if(CloseHandle(connections[curThread].Thread) == FALSE){
        printf("close %d Handle failed\n", curThread);
        return 1;
    }
    if(closesocket(connections[curThread].connec) != 0){
        printf("close %d socket failed\n", curThread);
        return 1;
    }

    int i; //, j;
    for(i = 0; connections[i].connec != '\0' && i < (number_connection - 1); i++){ //i is going to be infinite if all connecs exist
        connections[curThread + i] = connections[curThread + i + 1];
    }
    //i--;
    //for(j = 0; connections[j].Thread != NULL; j++){
    //    ;
    //}

    //connections[curThread].connec = connections[curThread + 1].connec;
    //connections[curThread].Thread = connections[curThread + 1].Thread;
    //connections[curThread].ip = connections[curThread + 1].ip;
    //connections[curThread].port = connections[curThread + 1].port;
    //i ist die Anzahl(>=0) <=2048
    //curThread >= 0 <=2047
    //
    //int a;

    //for(a = 0; a <= i - curThread; a++){
    //    connections[curThread + a].connec = connections[curThread + a + 1].connec;
    //    connections[curThread + a].Thread = connections[curThread + a + 1].Thread;
    //}
    //if((curThread + a ))
    //connections[curThread + a].connec = '\0';
    //connections[curThread + a].Thread = NULL;
    //connections[curThread + a].port = 0;

    numTh--;
    return 0;
}

unsigned int __stdcall process(void *arglist)
{
    int curThread = (int)arglist;
    HANDLE currentH = connections[curThread].Thread;

    connections[curThread].connec = ssock;

    //read the header from client

    connections[curThread].ip = fsin.sin_addr;
    connections[curThread].port = fsin.sin_port;

    //send back the asked content with a header

    //printf("%d\n", curThread);
    deleteClient(curThread);
    return 0;
}


int main()
{
    for(int i = 0; i <= (number_connection - 1); i++){
        connections[i].connec = '\0';
        connections[i].Thread = NULL;
        connections[i].port = 0;
    }

    printf("Hello World\n");
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(1, 1), &wsa) != 0){
        return 1;
    }
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN serverAddr;

    int port;
    char *buf = NULL;

    scanf("%d", &port);

    ZeroMemory(&serverAddr, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listen_sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    listen(listen_sock, 5);

    int threadID;
    int len = sizeof(SOCKADDR);
    for(numTh = 0; ; numTh++){
        ssock = accept(listen_sock, (struct sockaddr *)&fsin, &len);
        connections[numTh].Thread = (HANDLE)_beginthreadex(NULL, 0, process, (void *)numTh, 0, &threadID);
        //printf("%d.: %d\n", numTh, connections[numTh].Thread);
    }
    closesocket(listen_sock);

    closesocket(ssock);

    for(int i = 0; i < (number_connection - 1); i++){
        CloseHandle(connections[i].Thread);
        closesocket(connections[i].connec);
    }

    WSACleanup();
    return 0;
}