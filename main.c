#pragma comment(lib,"ws2_32.lib")

#include <stdio.h>
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <Ws2tcpip.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>


#define number_connection 100
const char *DIR = "D:\\Documents\\Project\\C++\\testWebsite";
const char *INDEX = "index.html";


void CALLBACK APCf(ULONG_PTR);
int readHeader(char *, struct requestH *, int *);
int createURL(char *);
char *response(int *, int, BOOLEAN *, struct responseH *);
int addToList(int, SOCKET, SOCKADDR_STORAGE *);
char *readFile(int *, char *, int *);
int toIP(SOCKADDR_STORAGE *, char *);
int endTask(int, int);
unsigned int __stdcall process(void *);


struct connection
{
    SOCKET connec[10];
    HANDLE Thread;
    SOCKADDR_STORAGE *client[10];
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

struct connection connections[number_connection];

void CALLBACK APCf(ULONG_PTR param){}

int readHeader(char *reqHeader, struct requestH *reqH, int * errC)
{
    char *line = NULL;
    char *next_line = NULL;

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
    if(reqH->file == NULL || reqH->protocol == NULL || reqH->method == NULL){
        *errC = 400;
    }
    return 0;
}

int createURL(char *wURL)
{
    if(wURL == NULL){
        return 1;
    }
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

char *response(int *hSize, int errC, BOOLEAN *bodyN, struct responseH *resH)
{
    char *header;
    *hSize = 1024;
    
    resH->status = errC;

    switch(errC){
        case 200:
            resH->res = "OK";
            *bodyN = TRUE;
            break;
        
        case 400:
            resH->res = "Bad Request";
            *bodyN = FALSE;
            break;

        case 404:
            resH->res = "Not Found";
            *bodyN = FALSE;
            break;
        
        default:
            resH->status = 500;
            *bodyN = FALSE;
            resH->res = "Internal Server Error";
            break;
    }

    resH->protocol = "HTTP/1.1";
    header = (char *)malloc(*hSize);
    ZeroMemory(header, *hSize);
    snprintf(header, 1024, "%s %d %s\r\nConnection: keep-alive", resH->protocol, resH->status, resH->res);

    return header;
}

int addToList(int curThread, SOCKET csock, SOCKADDR_STORAGE *client)
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

char *readFile(int *fSize, char *rurl, int *errC)
{
    errno_t errc;

    FILE *f;
    char *url = (char *)malloc(200);
    char *file = NULL;

    int test = snprintf(url, 200,"%s%s", DIR, rurl);

    errc = fopen_s(&f, url, "r");

    if(f == NULL){
        file = NULL;
        free(url);
        *errC = 404;
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
            *errC = 500;
        }
        else{
            fread_s((void *)file, *fSize, *fSize, 1, f);
        }
    }
    fclose(f);
    free(url);
    return file;
}

int toIP(SOCKADDR_STORAGE *storage, char *ip)
{
    ZeroMemory(ip, 50);
    switch(storage->ss_family){
        case AF_INET:
            SOCKADDR_IN client = *(struct sockaddr_in *)storage;
            inet_ntop(AF_INET, &(client.sin_addr), ip, 50);
            break;

        case AF_INET6:
            SOCKADDR_IN6 client6 = *(struct sockaddr_in6 *)storage;
            inet_ntop(AF_INET6, &(client6.sin6_addr), ip, 50);
            break;

        default:
            return 1;
    }
    return 0;
}

int endTask(int curThread, int task)
{
    if(closesocket(connections[curThread].connec[task]) != 0){
            printf("Err closing %d socket: %d", curThread, WSAGetLastError());
            return 1;
    }
    connections[curThread].connec[task] = '\0';

    return 0;
}

unsigned int __stdcall process(void *arglist)
{
    int curThread = *(int *)arglist;
    int stoppoint = 0;
    int fSize = 0;
    int hSize = 0;
    int errC = 200;

    BOOLEAN bodyN = TRUE;

    char *header = NULL;
    char *file = NULL;
    char ip[50];
    char date[9];
    char time[9];
    char request[2048];

    SleepEx(INFINITE, TRUE);

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

        toIP(connections[curThread].client[task], ip);

        _tzset();
        _strdate_s(date, 9);
        _strtime_s(time, 9);

        printf("%s %s | %s | %d | %d |\n", date, time, ip, curThread, task);
        ZeroMemory(request, sizeof(request));
        if(recv(connections[curThread].connec[task], request, sizeof(request), 0) <= 0){
            printf("Connection Broke.\n");
            if(endTask(curThread, task) != 0){
                printf("Err closing %d task of %d Thread: %d\n", task, curThread, WSAGetLastError());
            }
            continue;
        }

        struct responseH resH;
        struct requestH reqH;

        readHeader(request, &reqH, &errC);     //red the header and put athe data into reqH
        if(errC != 400){
            createURL(reqH.file);
            file = readFile(&fSize, reqH.file, &errC);
        }
        header = response(&hSize, errC, &bodyN, &resH);

        if(bodyN == FALSE){
            if(file != NULL){
                free(file);
            }
            file = "";
        }

        char *res = (char *)malloc(hSize + fSize + 10);
        ZeroMemory(res, hSize + fSize + 10);

        snprintf(res, hSize + fSize + 10, "%s\r\n\r\n%s", header, file);

        if(send(connections[curThread].connec[task], res, strlen(res), 0) == SOCKET_ERROR){
            printf("Err sending msg: %d", WSAGetLastError());
        }

        if(file != NULL){
            if(strlen(file) != 0){
                free(file);
            }
        }
        
        free(header);
        free(res);

        if(endTask(curThread, task) != 0){
            printf("Err closing %d task of %d Thread: %d\n", task, curThread, WSAGetLastError());
        }
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
    SOCKET listen_sock6 = socket(AF_INET6, SOCK_STREAM, 0);

    int optVal = 0;
    int optlen = sizeof(int);
    setsockopt(listen_sock6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&optVal, optlen);

    SOCKADDR_IN6 serverAddr6;

    ZeroMemory(&serverAddr6, sizeof(serverAddr6));

    int port = 0;
    printf("Please enter the port:\n");
    scanf_s("%d", &port);

    serverAddr6.sin6_family = AF_INET6;
    serverAddr6.sin6_port = htons(port);
    serverAddr6.sin6_addr = in6addr_any;

    if(bind(listen_sock6, (struct sockaddr *)&serverAddr6, sizeof(serverAddr6)) == SOCKET_ERROR){
        printf("Err binding the socket: %d\n", WSAGetLastError());
        main();
    }

    if(listen(listen_sock6, SOMAXCONN) == SOCKET_ERROR){
        printf("Err setting listen socket: %d\n", WSAGetLastError());
        main();
    }

    unsigned int threadID;

    int retryS = 0;
    int retryT = 0;

    int argList[number_connection];
    for(int curThread = 0; curThread < number_connection; curThread++){ //initialize the Threads
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
    SOCKADDR_STORAGE client;

    int len = sizeof(SOCKADDR_STORAGE);

    for(int i = 0; ; i++){
        srand(t + i);
        curThread = (rand() % number_connection);

        csock = accept(listen_sock6, (struct sockaddr *)&client, &len);

        if(csock == INVALID_SOCKET){
            printf("Err accepting a connection: %d\n", WSAGetLastError());
            continue;
        }
    
        if(addToList(curThread, csock, &client) != 0){
            printf("Err assigning a task\n");
            continue;
        }
    }

    closesocket(listen_sock6);
    for(int i = 0; i < number_connection; i++){
        CloseHandle(connections[i].Thread);
        for(int j = 0; j <= 9; j++){
            closesocket(connections[i].connec[j]);
        }
    }

    WSACleanup();
    return 0;
}