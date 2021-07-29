#pragma comment(lib, "advapi32.lib")
#pragma comment(lib,"ws2_32.lib")

#include <stdio.h>
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <Ws2tcpip.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>


#define number_connection 101 //1 bigger than actual
const char *DIR = "D:\\Documents\\Project\\C++\\testWebsite\\";

struct connection
{
    SOCKET connec[10];
    HANDLE Thread;
    IN_ADDR ip;
    int port;
    struct sockaddr_in client;
};

struct requestH
{
    char *method;
    char *protocol;
    char *file;
    char *Host;
};

struct responseH
{
    char *protocol;
    int status;
    char *res;
};

struct connection connections[number_connection - 1];

void CALLBACK APCf(ULONG_PTR param){}

int response(char *res, char *resB, struct responseH *resH)
{
    resH->status = 200;
    resH->res = "OK";
    resH->protocol = "HTTP/1.1";
    if(snprintf(res, 2000000,"%s %d %s\r\nConnection: closed\r\n\r\n%s", resH->protocol, resH->status, resH->res, resB) > 0){
        return 0;
    }
    else{
        return 1;
    }
}

int addToList(int curThread, SOCKET csock)
{
    int n = 1;
    for(int i = 0; i <= 9; i++){
        if(connections[curThread].connec[i] == '\0'){
            connections[curThread].connec[i] = csock;
            n = 0;
            break;
        }
    }
    QueueUserAPC(APCf, connections[curThread].Thread, (ULONG_PTR)NULL);
    return n;
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
        fread_s(file, 1000000, 2048, 1, f);
    }
    fclose(f);
    free(url);
    return 0;
}

unsigned int __stdcall process(void *arglist)
{
    int curThread = (int)arglist;

    int stoppoint = 0;

    SleepEx(INFINITE, TRUE);
    //connections[curThread].connec = ssock;
    char *file;

    for(int task = 0; ; task++){
        if(task > 9){
            task = 0;
        }
        if(connections[curThread].connec[task] == '\0'){
            if(task == stoppoint){
                SleepEx(INFINITE, TRUE);
            }
            continue;
        }
        stoppoint = task;
        printf("Thread Nr. %d: Task Nr.: %d\n", curThread,task);
        char request[2048];
        if(recv(connections[curThread].connec[task], request, 2048, 0) <= 0){
            printf("Bad Request\n");
            continue;
        }
        file = (char *)malloc(1000000);
        readFile(file, strdup("index.html"));

        char *res = (char *)malloc(2000000);
        struct responseH resH;
        response(res, file, &resH);
        send(connections[curThread].connec[task], res, 2048, 0);
        //send back the asked content with a header
        free(file);

        if(closesocket(connections[curThread].connec[task]) != 0){
            printf("Err closing %d socket: %d", curThread, WSAGetLastError());
        }
        connections[curThread].connec[task] = '\0';
    }

    if(CloseHandle(connections[curThread].Thread) == FALSE){
        printf("Err closing %d thread: %d", curThread, GetLastError());
    }
    _endthreadex(0);
    return 0;
}

int main()
{
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(1, 1), &wsa) != 0){
        return 1;
    }
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN serverAddr;

    ZeroMemory(&serverAddr, sizeof(serverAddr));

    int port = 0;
    printf("Please enter the port:\n");
    scanf_s("%d", &port);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listen_sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    listen(listen_sock, 5);

    unsigned int threadID;
    int len = sizeof(SOCKADDR);

    for(int curThread = 0; curThread < (number_connection - 1); curThread++){ //initialize the Threads
        memset(connections[curThread].connec, '\0', sizeof(SOCKET) * 10);
        //connections[curThread].Thread = NULL;
        connections[curThread].port = 0;
        connections[curThread].Thread = (HANDLE)_beginthreadex(NULL, 0, process, (void *)curThread, 0,  &threadID);
    }

    int t = (unsigned)time(NULL);
    int curThread = 0;
    SOCKET csock;
    for(int i = 0; ; i++){
        srand(t + i);
        curThread = (rand() % (number_connection - 1));

        csock = accept(listen_sock, (struct sockaddr *)&connections[curThread].client, &len);
    
        if(addToList(curThread, csock) != 0){
            printf("Err assigning a task\n");
            continue;
        }
    }

    closesocket(listen_sock);
    for(int i = 0; i < (number_connection - 1); i++){
        CloseHandle(connections[i].Thread);
        for(int j = 0; j <= 9; j++){
            closesocket(connections[i].connec[j]);
        }
    }

    WSACleanup();
    return 0;
}