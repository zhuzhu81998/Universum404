#include <stdio.h>
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <Ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib,"ws2_32.lib")

#define number_connection 2049 //1 bigger than actual
const char *DIR = "D:\\Documents\\Project\\C++\\testWebsite\\";

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
    for(i = 0; connections[i].connec != '\0' && i < (number_connection - 1); i++){ //could be improved
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

int readFile(char *file, char *rurl)
{
    errno_t errc;

    FILE *f;
    char *url = (char *)malloc(200);

    int test = snprintf(url, 200,"%s%s", DIR, rurl);

    errc = fopen_s(&f, url, "r");

    //ZeroMemory(file, sizeof(file));
    if(f == NULL){
        printf("FAILURE");
        return 1;
    }
    else{
        //do {
        //    printf("fast\n");
        //    c = fgetc(f);
        //    printf("not anymore\n");
        //    if(sprintf(file, "%s%c", file, c) < 0){
        //        printf("Failure");
        //    }
        //    printf("idk what imdoing");
        //} while(c != EOF);
        fread_s(file, 1000000, 2048, 1, f);
    }
    fclose(f);
    free(url);
    return 0;
}

unsigned int __stdcall process(void *arglist)
{
    int curThread = (int)arglist;

    connections[curThread].connec = ssock;

    //read the header from client

    connections[curThread].ip = fsin.sin_addr;
    connections[curThread].port = fsin.sin_port;

    char *file = malloc(1000000);
    readFile(file, strdup("index.html"));

    send(connections[curThread].connec, file, 2048, 0);
    //send back the asked content with a header

    //printf("%d\n", curThread);
    deleteClient(curThread);
    free(file);
    _endthreadex(0);
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

    unsigned int threadID;
    int len = sizeof(SOCKADDR);
    for(numTh = 0; ; numTh++){
        ssock = accept(listen_sock, (struct sockaddr *)&fsin, &len);
        connections[numTh].Thread = (HANDLE)_beginthreadex(NULL, 0, process, (void *)numTh, 0, &threadID);
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