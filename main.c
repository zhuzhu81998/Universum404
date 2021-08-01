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
int binsprintf(char **buf, char *str1, size_t len1, char *str2, size_t len2);
int readHeader(char *, struct requestH *, int *);
int createURL(char *);
int response(char **header, int *hSize, int errC, BOOLEAN *bodyN, struct responseH *resH);
int addToList(int, SOCKET, SOCKADDR_STORAGE *);
int readFile(char **file, long *fSize, char *rurl, int *errC);
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
    long fSize;
    char *fType;
};

struct connection connections[number_connection];

void CALLBACK APCf(ULONG_PTR param){}

int binsprintf(char **buf, char *str1, size_t len1, char *str2, size_t len2)
{
    if(str1 == NULL || str2 == NULL){
        printf("Err wrong pointer\n");
        return 1;
    }
    *buf = (char *)malloc(len1 + len2);
    if(*buf == NULL){
        printf("Err initiating memory for reading files\n");
    }
    ZeroMemory(*buf, len1 + len2);
    int i = 0, j = 0;
    for(i = 0; i < len1; i++){
        (*buf)[i] = str1[i];
    }
    for(j = 0; j < len2; j++){
        (*buf)[i + j] = str2[j];
    }
    return 0;
}

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

int response(char **header, int *hSize, int errC, BOOLEAN *bodyN, struct responseH *resH)
{
    *hSize = 1024;
    int len = 0;
    
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
    *header = (char *)malloc(*hSize);
    ZeroMemory(*header, *hSize);
    if(header == NULL){
        printf("Err initiating memory\n");
        resH->status = 500;
        return 1;
    }
    len = snprintf(*header, *hSize, "%s %d %s\r\nAccept-Ranges: bytes\r\n", resH->protocol, resH->status, resH->res);
    if(resH->fSize != 0){
        len = snprintf(*header, *hSize, "%sContent-Length: %ld\r\n", *header, resH->fSize);
    }
    len = snprintf(*header, *hSize, "%s\r\n", *header);

    *hSize = len;
    return 0;
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

int readFile(char **file, long *fSize, char *rurl, int *errC)
{
    errno_t errc;

    FILE *f;
    char *url = (char *)malloc(200);

    int test = snprintf(url, 200, "%s%s", DIR, rurl);
    errc = fopen_s(&f, url, "rb");

    if(f == NULL){
        *file = NULL;
        free(url);
        *errC = 404;
        return 1;
    }
    else{
        fseek(f, 0, SEEK_END);
        *fSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        *file = (char *)malloc(*fSize);
        ZeroMemory(*file, *fSize);
        if(*file == NULL){
            printf("Err initiating memory for reading files\n");
            *errC = 500;
        }
        else{
            int check = 0;
            check = fread_s((void *)*file, *fSize, *fSize, 1, f);
        }
    }
    fclose(f);
    free(url);
    return 0;
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
            return 1;
    }
    connections[curThread].connec[task] = '\0';

    return 0;
}

unsigned int __stdcall process(void *arglist)
{
    int curThread = *(int *)arglist;
    int stoppoint = 0;
    long fSize = 0;
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

        //set timeout for recv
        DWORD timeout = 4500;
        if(setsockopt(connections[curThread].connec[task], SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(DWORD)) != 0 ){
            printf("Err setting timeout: %d\n", WSAGetLastError());
            if(endTask(curThread, task) != 0){
                printf("Err closing %d task of %d Thread: %d\n", task, curThread, WSAGetLastError());
            }
        }

        if(recv(connections[curThread].connec[task], request, sizeof(request), 0) <= 0){
            printf("Connection Broke.\n");
            if(endTask(curThread, task) != 0){
                printf("Err closing %d task of %d Thread: %d\n", task, curThread, WSAGetLastError());
            }
            continue;
        }

        struct responseH resH;
        struct requestH reqH;

        resH.fSize = 0;
        resH.protocol = "HTTP/1.1";
        resH.status = -1;
        resH.res = NULL;

        readHeader(request, &reqH, &errC);     //read the header and put the data into reqH
        if(errC != 400 && bodyN == TRUE){
            createURL(reqH.file);
            //resH.fType = "image/jpeg";
            readFile(&file, &fSize, reqH.file, &errC);
            resH.fSize = fSize;
        }
        if(errC == 404 || bodyN == FALSE){
            file = "";
            fSize = 0;
        }

        response(&header, &hSize, errC, &bodyN, &resH);
        char *res = NULL;
        binsprintf(&res, header, hSize, file, fSize);

        int byteS = send(connections[curThread].connec[task], res, hSize + fSize, 0);
        if(byteS == SOCKET_ERROR){
            printf("Err sending msg: %d\n", WSAGetLastError());
        }

        if(file != NULL){
            if(fSize != 0){
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
        printf("Err closing %d thread: %d\n", curThread, GetLastError());
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