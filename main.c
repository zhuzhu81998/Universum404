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
//#define number_process 10
#define max_request_header_size 8000 //in bytes

const char *protocol = "HTTP/1.1";
const char *doc400 = NULL;  //this file names must begin with "\\"
const char *doc404 = NULL;
const char *doc413 = NULL;
const char *doc500 = NULL;
const char *DIR = "D:\\Documents\\Project\\C++\\testWebsite";
const char *INDEX = "index.html";
char *mimeTypes = NULL;


void CALLBACK APCf(ULONG_PTR);
int binsprintf(char **buf, char *str1, size_t len1, char *str2, size_t len2);
int readHeader(char *reqHeader, struct requestH *reqH, struct response *res);
int createURL(char *);
int response(struct response *res);
int addToList(int, SOCKET, SOCKADDR_STORAGE *);
int readFile(struct response *res, char *rurl);
int toIP(SOCKADDR_STORAGE *, char *);
int endTask(int, int);
int fileForErr(struct response *res);
int resErr(struct response *res);
int sendRes(struct response res);
unsigned int __stdcall Thread(void *);


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
    BOOLEAN ifcgi;  //denote whether cgi or not
    char *querystring;
};

struct response
{
    char *protocol;
    int status;
    char *res;
    long long fSize;
    char *fType;
    char *body;
    char *header;
    char *mimeType;
    int hSize;
    SOCKET sock;
    BOOLEAN bodyN;  //denote whether a real body is required
};

struct connection connections[number_connection];

void CALLBACK APCf(ULONG_PTR param){}

int checkType(struct response *res, char *url)
{
    int len = strlen(url);
    int i;
    for(i = (len - 1); i >= 0; i--){
        if(url[i] == '.'){
            break;
        }
    }
    char ext[10];
    ZeroMemory(ext, 10);
    for(int j = 0; j < len - i; j++){
        ext[j] = url[j + i];
    }
    char *line = NULL;
    line = strstr(mimeTypes, ext);
    if(line == NULL){
        printf("Err finding the Type\n");
        res->mimeType = NULL;
        return 1;
    }
    int linelen;
    for(linelen = 0; line[linelen] != ';'; linelen++){}
    for(i = 0; i < linelen; i++){
        if(line[i] == '='){
            i++;
            break;
        }
    }
    res->mimeType = (char *)malloc(linelen);
    ZeroMemory(res->mimeType, linelen);
    for(int j = 0; j < linelen && line[i + j] != ';'; j++){
        res->mimeType[j] = line[i + j];
    }
    return 0;
}

int initTypes()
{
    FILE *f;

    fopen_s(&f, "mime.types", "rt");

    if(f == NULL){
        return 1;
    }
    else{
        _fseeki64(f, 0, SEEK_END);
        int fSize = _ftelli64(f);
        _fseeki64(f, 0, SEEK_SET);

        mimeTypes = (char *)malloc(fSize + 1);
        ZeroMemory(mimeTypes, fSize + 1);
        if(mimeTypes == NULL){
            printf("Err initiating memory for reading mime.types\n");
            return 1;
        }
        else{
            int check = 0;
            check = fread_s((void *)mimeTypes, fSize, fSize, 1, f);
            mimeTypes[fSize] = '\0';
        }
    }
    fclose(f);
    return 0;
}

int binsprintf(char **buf, char *str1, size_t len1, char *str2, size_t len2)
{
    if(str1 == NULL || str2 == NULL){
        printf("Err wrong pointer: \"%s\" and \"%s\"\n", str1, str2);
        return 500;
    }
    *buf = (char *)malloc(len1 + len2);
    if(*buf == NULL){
        printf("Err initiating memory for reading files\n");
        return 500;
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

int readHeader(char *reqHeader, struct requestH *reqH, struct response *res)
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
        res->status = 400;
        resErr(res);
        return 400;
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

int response(struct response *res)
{
    res->hSize = 1024;
    int len = 0;

    res->protocol = _strdup(protocol);
    res->header = (char *)malloc(res->hSize);
    ZeroMemory(res->header, res->hSize);
    if(res->header == NULL){
        printf("Err initiating memory\n");
        res->status = 500;
        return 500;
    }
    len = snprintf(res->header, res->hSize, "%s %d %s\r\nAccept-Ranges: bytes\r\n", res->protocol, res->status, res->res);
    if(res->mimeType != NULL){
        len = snprintf(res->header, res->hSize, "%sContent-Type: %s; charset=utf-8\r\n", res->header, res->mimeType);
    }
    if(res->fSize != 0){
        len = snprintf(res->header, res->hSize, "%sContent-Length: %lld\r\n", res->header, res->fSize);
    }
    len = snprintf(res->header, res->hSize, "%s\r\n", res->header);

    res->hSize = len;
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

int readFile(struct response *res, char *rurl)
{
    FILE *f = NULL;
    size_t urllen = strlen(DIR) + strlen(rurl);
    char *url = (char *)malloc(urllen + 1);
    ZeroMemory(url, urllen + 1);

    int test = snprintf(url, urllen + 1, "%s%s", DIR, rurl);

    fopen_s(&f, url, "rb, ccs=utf-8");

    if(f == NULL){
        free(url);
        res->status = 404;
        resErr(res);
        return 404;
    }
    else{
        _fseeki64(f, 0, SEEK_END);
        res->fSize = _ftelli64(f);
        _fseeki64(f, 0, SEEK_SET);

        res->body = (char *)malloc(res->fSize);
        ZeroMemory(res->body, res->fSize);
        if(res->body == NULL){
            printf("Err initiating memory for reading files\n");
            res->status = 500;
            resErr(res);
            return 500;
        }
        else{
            int check = 0;
            check = fread_s((void *)res->body, res->fSize, res->fSize, 1, f);
            checkType(res, url);
        }
        free(url);
    }
    fclose(f);
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

int fileForErr(struct response *res)
{
    switch(res->status){
        case 400:
            if(doc400 != NULL){
                readFile(res, _strdup(doc400));
            }
            else{
                res->body = "";
                res->fSize = 0;
            }
            break;

        case 404:
            if(doc404 != NULL){
                readFile(res, _strdup(doc404));
            }
            else{
                res->body = "";
                res->fSize = 0;
            }
            break;

        case 413:
            if(doc413 != NULL){
                readFile(res, _strdup(doc413));
            }
            else{
                res->body = "";
                res->fSize = 0;
            }
            break;

        case 500:
            if(doc500 != NULL){
                readFile(res, _strdup(doc500));
            }
            else{
                res->body = "";
                res->fSize = 0;
            }
            break;
        
        default:
            if(doc500 != NULL){
                readFile(res, _strdup(doc500));
            }
            else{
                res->body = "";
                res->fSize = 0;
            }
            break;
    }
    return 0;
}

int resErr(struct response *res)
{
    switch(res->status){
        case 400:
            res->res = "Bad Request";
            res->bodyN = FALSE;
            break;

        case 404:
            res->res = "Not Found";
            res->bodyN = FALSE;
            break;

        case 413:
            res->res = "Request Entity Too Large";
            res->bodyN = FALSE;
        
        default:
            res->status = 500;
            res->res = "Internal Server Error";
            res->bodyN = FALSE;
            break;
    }
    fileForErr(res);
    return 0;
}

int sendRes(struct response res)
{
    char *resMsg = NULL;
    binsprintf(&resMsg, res.header, res.hSize, res.body, res.fSize);

    int byteS = send(res.sock, resMsg, res.hSize + res.fSize, 0);
    if(byteS == SOCKET_ERROR){
        printf("Err sending msg: %d\n", WSAGetLastError());
    }

    if(res.body != NULL){
        if(res.fSize != 0){
            free(res.body);
        }
    }
    if(res.mimeType != NULL){
        free(res.mimeType);
    }
    free(res.header);
    free(resMsg);
    return 0;
}

unsigned int __stdcall Thread(void *arglist)
{
    int curThread = *(int *)arglist;
    int stoppoint = 0;

    struct response res;
    struct requestH reqH;

    char ip[50];
    char date[9];
    char time[9];
    char request[max_request_header_size];

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

        res.fSize = 0;
        res.protocol = _strdup(protocol);
        res.status = 200;
        res.res = NULL;
        res.body = NULL;
        res.header = NULL;
        res.hSize = 0;
        res.sock = connections[curThread].connec[task];
        res.bodyN = TRUE;
        res.mimeType = NULL;

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

        int recvV = recv(connections[curThread].connec[task], request, sizeof(request), 0);
        switch(recvV){
            case 0:
                printf("Connection Broke.\n");
                if(endTask(curThread, task) != 0){
                    printf("Err closing %d task of %d Thread: %d\n", task, curThread, WSAGetLastError());
                }
                continue;
                break;
            
            case SOCKET_ERROR:
                int errWSA = WSAGetLastError();
                printf("Err receiving request header: %d\n", errWSA);
                if(errWSA == WSAEMSGSIZE){
                    res.status = 413;
                    resErr(&res);
                }
                else{
                    if(endTask(curThread, task) != 0){
                        printf("Err closing %d task of %d Thread: %d\n", task, curThread, WSAGetLastError());
                    }
                    continue;
                }
                break;
        }

        if(res.status != 413){
            readHeader(request, &reqH, &res);     //read the header and put the data into reqH
        }

        if(res.bodyN == TRUE){
            createURL(reqH.file);
            readFile(&res, reqH.file);
        }

        response(&res);
        sendRes(res);

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
    if(initTypes() == 1){
        printf("Err reading mime.types\n");
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
        connections[curThread].Thread = (HANDLE)_beginthreadex(NULL, 0, Thread, (void *)&argList[curThread], 0,  &threadID);

        if(connections[curThread].Thread == 0){
            if(retryT < 2){
                printf("Err starting Nr. %d Thread\n", curThread);
                curThread--;
                retryT++;
                continue;
            }
            else{
                main();
            }
        }
    }

    int curThread = 0;
    SOCKET csock;
    SOCKADDR_STORAGE client;

    int len = sizeof(SOCKADDR_STORAGE);

    srand((unsigned)time(NULL));
    for( ; ; ){
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
    free(mimeTypes);
    WSACleanup();
    return 0;
}