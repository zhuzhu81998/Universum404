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

struct connection connections[number_connection - 1];

void CALLBACK APCf(ULONG_PTR param){}

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
        //read the header from client
        file = malloc(1000000);
        readFile(file, strdup("index.html"));

        send(connections[curThread].connec[task], file, 2048, 0);
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
    printf("Please enter the port:\n");
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(1, 1), &wsa) != 0){
        return 1;
    }
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN serverAddr;

    int port;

    scanf("%d", &port);

    ZeroMemory(&serverAddr, sizeof(serverAddr));

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