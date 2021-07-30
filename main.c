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
const char *DIR = "D:\\Documents\\Project\\C++\\testWebsite";
const char *INDEX = "index.html";

struct connection
{
    SOCKET connec[10];
    HANDLE Thread;
    IN_ADDR ip;
    int port;
    struct sockaddr_in client[10];
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

int readHeader(char *reqHeader, struct requestH *reqH)
{
    char *line = NULL;
    char *next_line = NULL;

    //memset(line, NULL, 50);
    char *word = NULL;
    char *next_word = NULL;

    int i = 1;
    line = strtok_s(reqHeader, "\r\n", &next_line);
    for(i = 1; line != NULL; i++){
        if(i == 1){
            word = strtok_s(line, " ", &next_word);
            reqH->method = word;
            for(int j = 1; word != NULL; j++){
                word = strtok_s(NULL, " ", &next_word);
                switch(j){
                    case 1:
                        reqH->file = word;
                        break;
                    case 2:
                        reqH->protocol = word;
                        break;
                    default:
                        break;
                }
            }
        }
        line = strtok_s(NULL, "\r\n", &next_line);
    }
    return 0;
}

int createURL(char *wURL)
{
    if(strcmp("/", wURL) == 0){
        snprintf(wURL, 25, "\\%s", INDEX);
    }
    else{
        int len = strlen(wURL);
        for(int i = 0; i < len; i++){
            if(wURL[i] == '/'){
                wURL[i] = '\\';
            }
        }
    }
    return 0;
}

char *response(int *hSize, struct responseH *resH)
{
    char *header;
    *hSize = 1024;
    resH->status = 200;
    resH->res = "OK";
    resH->protocol = "HTTP/1.1";
    header = (char *)malloc(*hSize);
    ZeroMemory(header, *hSize);
    snprintf(header, 1024, "%s %d %s\r\nConnection: keep-alive", resH->protocol, resH->status, resH->res);

    return header;
}

int addToList(int curThread, SOCKET csock, struct sockaddr_in client)
{
    int n = 1;
    for(int i = 0; i <= 9; i++){
        if(connections[curThread].connec[i] == '\0'){
            connections[curThread].connec[i] = csock;
            connections[curThread].client[i] = client;
            n = 0;
            break;
        }
    }
    QueueUserAPC(APCf, connections[curThread].Thread, (ULONG_PTR)NULL);
    return n;
}

char *readFile(int *fSize, char *rurl, int *err)
{
    *err = 0;
    errno_t errc;

    FILE *f;
    char *url = (char *)malloc(200);
    char *file = NULL;

    int test = snprintf(url, 200,"%s%s", DIR, rurl);

    errc = fopen_s(&f, url, "r");

    if(f == NULL){
        file = NULL;
        free(url);
        *err = 404;
        return file;
    }
    else{
        fseek(f, 0, SEEK_END);
        *fSize = ftell(f);
        rewind(f);

        file = (char *)malloc(*fSize);
        ZeroMemory(file, *fSize);
        if(file == NULL){
            printf("Err initiating memory for reading files\n");
            *err = 1;
        }
        else{
            fread_s((void *)file, *fSize, *fSize, 1, f);
        }
    }
    fclose(f);
    free(url);
    return file;
}

unsigned int __stdcall process(void *arglist)
{
    int curThread = *(int *)arglist;
    printf("%d\n", curThread);
    int stoppoint = 0;

    SleepEx(INFINITE, TRUE);
    int fSize = 0;
    int hSize = 0;
    char *header = NULL;
    char *file = NULL;

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
        printf("IP %s: Thread Nr. %d: Task Nr.: %d\n", inet_ntoa(connections[curThread].client[task].sin_addr), curThread,task);
        char request[2048];
        if(recv(connections[curThread].connec[task], request, 2048, 0) <= 0){
            printf("Connection Broke.\n");
            if(closesocket(connections[curThread].connec[task]) != 0){
                printf("Err closing %d socket: %d", curThread, WSAGetLastError());
            }
            connections[curThread].connec[task] = '\0';
            continue;
        }

        struct responseH resH;
        struct requestH reqH;

        readHeader(request, &reqH);     //read the header and put the data into reqH
        
        createURL(reqH.file);

        header = response(&hSize, &resH);
        int errF;
        file = readFile(&fSize, reqH.file, &errF);

        if(errF == 404){
            //not_found();
            printf("file not found\n");
            continue;
        }

        char *res = (char *)malloc(hSize + fSize + 10);
        ZeroMemory(res, hSize + fSize + 10);

        snprintf(res, hSize + fSize + 10, "%s\r\n\r\n%s", header, file);

        if(send(connections[curThread].connec[task], res, strlen(res), 0) == SOCKET_ERROR){
            printf("Err sending msg: %d", WSAGetLastError());
        }

        free(file);
        free(header);

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
    system("cls");
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

    if(bind(listen_sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR){
        printf("Err binding the socket: %d\n", WSAGetLastError());
        main();
    }

    if(listen(listen_sock, 5) == SOCKET_ERROR){
        printf("Err setting listen socket: %d\n", WSAGetLastError());
        main();
    }

    unsigned int threadID;
    int len = sizeof(SOCKADDR);
    int retryS = 0;
    int retryT = 0;

    int argList[number_connection - 1];
    for(int curThread = 0; curThread < (number_connection - 1); curThread++){ //initialize the Threads
        if(memset(connections[curThread].connec, '\0', sizeof(SOCKET) * 10) == NULL){
            if(retryS < 2){
                printf("Err initiating sockets\n");
                curThread--;
                retryS++;
                continue;
            }
            else{
                main();
            }
        }

        connections[curThread].port = 0;
        argList[curThread] = curThread;
        connections[curThread].Thread = (HANDLE)_beginthreadex(NULL, 0, process, (void *)&argList[curThread], 0,  &threadID);

        if(connections[curThread].Thread == 0){
            if(retryT < 2){
                printf("Err starting Nr. %d Thread\n", curThread);
                curThread--;
                continue;
            }
            else{
                main();
            }
        }
    }

    int t = (unsigned)time(NULL);
    int curThread = 0;
    SOCKET csock;
    struct sockaddr_in client;

    for(int i = 0; ; i++){
        srand(t + i);
        curThread = (rand() % (number_connection - 1));

        csock = accept(listen_sock, (struct sockaddr *)&client, &len);

        if(csock == INVALID_SOCKET){
            printf("Err accepting a connection: %d\n", WSAGetLastError());
            continue;
        }
    
        if(addToList(curThread, csock, client) != 0){
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