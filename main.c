#pragma comment(lib,"ws2_32.lib")

#include <stdio.h>
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#include <Ws2tcpip.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>


#define number_connection 20   //pro process
#define number_process 5
#define number_task 10
#define max_request_header_size 8000 //in bytes
#define max_index_length 30

const char *protocol = "HTTP/1.1";
const char *doc400 = NULL;  //this file names must begin with "\\"
const char *doc404 = NULL;
const char *doc413 = NULL;
const char *doc500 = NULL;
const char *DIR = "D:\\Documents\\Project\\C++\\testWebsite";
const char *INDEX = "index.html";
const char *SERVER = "Universum404/0.0.1";
char *mimeTypes = NULL;
char *self = NULL;

HANDLE parent_main_thread = NULL;

int m_port = 0;
SOCKET m_listen_sock6;
PROCESS_INFORMATION m_pi[number_process];


void CALLBACK APCf(ULONG_PTR param);
int setEnv(char **env, struct requestH *reqH, struct response *res);
int getInterpreter(struct requestH *reqH);
int setCGImimeType(struct requestH *reqH, struct response *res);
int getQuery(struct requestH *reqH);
int execute_cgi(struct requestH *reqH, struct response *res);
int getExt(char *url, char **ext);
int checkType(struct requestH *reqH, struct response *res);
int initTypes();
int bincat(char **des, size_t *lendes, char *str, size_t lenstr);
int binsprintf(char **buf, char *str1, size_t len1, char *str2, size_t len2);
int readHeader(char *reqHeader, struct requestH *reqH, struct response *res);
int createURL(struct requestH *reqH);
int response(struct response *res);
int addToList(int curThread, SOCKET csock, SOCKADDR_STORAGE *client);
int readFile(struct response *res, char *url);
int toIP(SOCKADDR_STORAGE *storage, char *ip);
int endTask(int curThread, int task);
int fileForErr(struct response *res);
int resErr(struct response *res);
int sendRes(struct response res);
unsigned int __stdcall Thread(void *arglist);
unsigned int __stdcall child_wait(void *arglist);
int child_main(int argc, char **argv);
int createChildProcess(int pID);
void CALLBACK restart_Process(ULONG_PTR param);
unsigned int __stdcall parent_wait(void *arglist);
int parent_main(int argc, char **argv);


struct connection
{
    SOCKET connec[number_task];
    HANDLE Thread;
    SOCKADDR_STORAGE *client[number_task];
};

struct requestH
{
    char *method;
    char *protocol;
    char *file;
    char *uri;
    char *Host;
    char *query;
    char *cgi_interpreter;
    char *body;
    int Thread;
    int task;
    long long bSize;
    char *ext;
    BOOLEAN ifcgi;
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

int setEnv(char **env, struct requestH *reqH, struct response *res)
{
    *env = calloc(100, sizeof(char));
    char *buf = calloc(100, sizeof(char));
    int len1 = 0;
    int len2 = 0;

    len1 = snprintf(*env, 100, "SERVER_SOFTWARE=%s", SERVER) + 1;
    len2 = snprintf(buf, 100, "REQUEST_METHOD=%s", reqH->method) + 1;
    bincat(env, &len1, buf, len2);
    ZeroMemory(buf, 100);

    len2 = snprintf(buf, 100, "QUERY_STRING=%s", reqH->query) + 1;
    bincat(env, &len1, buf, len2);
    ZeroMemory(buf, 100);

    len2 = snprintf(buf, 100, "GATEWAY_INTERFACE=CGI/1.1") + 1;
    bincat(env, &len1, buf, len2);

    bincat(env, &len1, "\0", 1);

    free(buf);
    return 0;
}

int getInterpreter(struct requestH *reqH)
{
    if(strcmp(reqH->ext, ".php") == 0){
        reqH->cgi_interpreter = "php";
        reqH->ifcgi = TRUE;
    }
    return 0;
}

int setCGImimeType(struct requestH *reqH, struct response *res)
{
    res->mimeType = calloc(15, sizeof(char));

    if(strcmp(reqH->ext, ".php") == 0){
        strcpy_s(res->mimeType, 15, "text/html");
    }
    return 0;
}

int getQuery(struct requestH *reqH)
{
    reqH->ifcgi = FALSE;
    int len = strlen(reqH->file);
    int i;
    for(i = 0; i < len; i++){
        if(reqH->file[i] == '?'){
            reqH->ifcgi = TRUE;
            i++;
            break;
        }
    }
    if(reqH->ifcgi == FALSE){
        reqH->query = NULL;
        return 1;
    }
    reqH->query = calloc(len - i + 1, sizeof(char));

    for(int j = 0; j < len - i; j++){
        reqH->query[j] = reqH->file[j + i];
    }
    reqH->query[len - i] = '\0';
    i--;
    reqH->file[i] = '\0';
    return 0;
}

int execute_cgi(struct requestH *reqH, struct response *res)
{
    //execute cgi script
    HANDLE std_OUT_Read = NULL;
    HANDLE std_OUT_Write = NULL;
    HANDLE std_IN_Read = NULL;
    HANDLE std_IN_Write = NULL;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    if((CreatePipe(&std_OUT_Read, &std_OUT_Write, &sa, 0) == FALSE) || (CreatePipe(&std_IN_Read, &std_IN_Write, &sa, 0) == FALSE)){
        res->status = 500;
        resErr(res);
        printf("Err creating Pipe\n");
        return 1;
    }
    if ((SetHandleInformation(std_OUT_Read, HANDLE_FLAG_INHERIT, 0) == FALSE) || (SetHandleInformation(std_IN_Write, HANDLE_FLAG_INHERIT, 0) == FALSE)){
        printf("Err setting handle information\n");
    }

    STARTUPINFO sui;
    sui.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&sui);
    sui.hStdError = std_OUT_Write;
    sui.hStdOutput = std_OUT_Write;
    sui.hStdInput = std_IN_Read;
    sui.dwFlags |= STARTF_USESTDHANDLES;

    if(stricmp(reqH->method, "POST") == 0 && reqH->body != 0){
        DWORD nWritten;
        WriteFile(std_IN_Write, reqH->body, strlen(reqH->body), &nWritten, NULL);
    }

    createURL(reqH);
    int len = strlen(reqH->file) + 1 + strlen(reqH->cgi_interpreter);
    char *cmd = calloc(len + 1, sizeof(char));
    snprintf(cmd, len + 1, "%s %s", reqH->cgi_interpreter, reqH->file);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    DWORD dwCreationFlags = CREATE_NO_WINDOW;
    
    char *env = NULL;
    setEnv(&env, reqH, res);
    if(CreateProcessA(NULL, cmd, NULL, NULL, TRUE, dwCreationFlags, env, NULL, &sui, &pi) == FALSE){
        printf("Err creating process: %d\n", GetLastError());
    }

    free(env);
    env = NULL;
    free(cmd);
    cmd = NULL;

    res->fSize = 0;

    DWORD nRead;
    DWORD Size = 0; 

    char bufRead[100];
    ZeroMemory(bufRead, 100);
    res->body = calloc(100, sizeof(char));

    BOOLEAN bSuccess = FALSE;
    
    if(std_OUT_Write != NULL){
        CloseHandle(std_OUT_Write);
        std_OUT_Write = NULL;
    }
    bSuccess = ReadFile(std_OUT_Read, res->body, 100, &Size, NULL);
    if(bSuccess == FALSE || Size == 0){
        if(GetLastError() != ERROR_MORE_DATA){
            printf("Err reading Pipe: %d\n", GetLastError());
            res->status = 500;
            resErr(res);
            return 1;
        }
    }
    while(TRUE){
        bSuccess = FALSE;
        bSuccess = ReadFile(std_OUT_Read, bufRead, 100, &nRead, NULL);
        if(bSuccess == FALSE || nRead == 0){
            break;
        }
        bincat(&(res->body), &Size, bufRead, nRead);
        ZeroMemory(bufRead, 100);
        nRead = 0;
    }
    res->fSize = Size;
    setCGImimeType(reqH, res);
    res->bodyN = FALSE;
    free(reqH->query);
    reqH->query = NULL;

    CloseHandle(std_OUT_Read);
    CloseHandle(std_IN_Read);
    CloseHandle(std_IN_Write);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

int getExt(char *url, char **ext)
{
    int len = strlen(url);
    int i;
    for(i = (len - 1); i >= 0; i--){
        if(url[i] == '.'){
            break;
        }
    }
    *ext = calloc(len - i + 1, sizeof(char));
    for(int j = 0; j < len - i; j++){
        (*ext)[j] = url[j + i];
    }
    return 0;
}

int checkType(struct requestH *reqH, struct response *res)
{
    char *line = NULL;
    line = strstr(mimeTypes, reqH->ext);
    if(line == NULL){
        printf("Err finding the Type\n");
        res->mimeType = NULL;
        return 1;
    }
    int linelen;
    for(linelen = 0; line[linelen] != ';'; linelen++){}
    int i;
    for(i = 0; i < linelen; i++){
        if(line[i] == '='){
            i++;
            break;
        }
    }
    res->mimeType = calloc(linelen, sizeof(char));
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

        mimeTypes = calloc(fSize + 1, sizeof(char));
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

int bincat(char **des, size_t *lendes, char *str, size_t lenstr)
{
    int newlen = (*lendes) + lenstr;
    char *buf = calloc(newlen, sizeof(char));

    int retVar = binsprintf(&buf, (*des), *lendes, str, lenstr);
    free(*des);
    (*des) = buf;
    (*lendes) = newlen;
    return retVar;
}

int binsprintf(char **buf, char *str1, size_t len1, char *str2, size_t len2)
{
    if(str1 == NULL || str2 == NULL){
        printf("Err wrong pointer: \"%s\" and \"%s\"\n", str1, str2);
        return 500;
    }
    *buf = calloc(len1 + len2, sizeof(char));
    if(*buf == NULL){
        printf("Err initiating memory for reading files\n");
        return 500;
    }
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
                        int len = strlen(word) + 1;
                        reqH->file = calloc(len + 1, sizeof(char));
                        strcpy_s(reqH->file, len, word);
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
    if(reqH->file != NULL){
        getQuery(reqH);
    }
    if(reqH->file == NULL || reqH->protocol == NULL || reqH->method == NULL){
        res->status = 400;
        resErr(res);
        return 400;
    }
    return 0;
}

int createURL(struct requestH *reqH)
{
    if(reqH->file == NULL){
        return 1;
    }
    
    int len = strlen(reqH->file);
    int urllen = len + strlen(DIR) + max_index_length + 1;
    char *url = calloc(urllen + 1, sizeof(char));

    for(int i = 0; i < len; i++){
        if(reqH->file[i] == '/'){
            reqH->file[i] = '\\';
        }
    }
    snprintf(url, urllen, "%s%s", DIR, reqH->file);

    DWORD dWord = GetFileAttributes(url);
    if(dWord == INVALID_FILE_ATTRIBUTES){
        free(url);
        url = NULL;
        return 404;
    }
    if(dWord & FILE_ATTRIBUTE_DIRECTORY){
        int urlstrlen = strlen(url);
        if(url[urlstrlen - 1] == '\\'){
            snprintf(url, urllen, "%s%s", url, INDEX);
        }
        else{
            snprintf(url, urllen, "%s\\%s", url, INDEX);
        }
    }
    free(reqH->file);
    reqH->file = NULL;
    reqH->file = url;
    return 0;
}

int response(struct response *res)
{
    res->hSize = 1024;
    int len = 0;

    res->protocol = _strdup(protocol);
    res->header = calloc(res->hSize, sizeof(char));
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
    for(int i = 0; i < number_task; i++){
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

int readFile(struct response *res, char *url)
{
    FILE *f = NULL;

    fopen_s(&f, url, "rb, ccs=utf-8");

    if(f == NULL){
        res->status = 404;
        resErr(res);
        return 404;
    }
    else{
        _fseeki64(f, 0, SEEK_END);
        res->fSize = _ftelli64(f);
        _fseeki64(f, 0, SEEK_SET);

        res->body = calloc(res->fSize, sizeof(char));
        if(res->body == NULL){
            printf("Err initiating memory for reading files\n");
            res->status = 500;
            resErr(res);
            return 500;
        }
        else{
            int check = 0;
            check = fread_s((void *)res->body, res->fSize, res->fSize, 1, f);
        }
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
    free(resMsg);
    resMsg = NULL;
    return 0;
}

unsigned int __stdcall Thread(void *arglist)
{
    int curThread = *(int *)arglist;
    int stoppoint = 0;
    DWORD timeout = 4500;

    char ip[50];
    char date[9];
    char time[9];
    char *request = calloc(max_request_header_size, sizeof(char));

    SleepEx(INFINITE, TRUE);

    for(int task = 0; ; task++){
        if(task > (number_task - 1)){
            task = 0;
        }
        if(connections[curThread].connec[task] == '\0'){
            if(task == stoppoint){
                SleepEx(INFINITE, TRUE);
            }
            continue;
        }
        stoppoint = task;

        struct response res;
        struct requestH reqH;

        ZeroMemory(&reqH, sizeof(struct requestH));
        reqH.task = task;
        reqH.Thread = curThread;

        ZeroMemory(&res, sizeof(struct response));
        res.protocol = _strdup(protocol);
        res.status = 200;
        res.res = "OK";
        res.sock = connections[curThread].connec[task];
        res.bodyN = TRUE;

        toIP(connections[curThread].client[task], ip);

        _tzset();
        _strdate_s(date, 9);
        _strtime_s(time, 9);

        printf("%s %s | %s | %d | %d | %lu |\n", date, time, ip, curThread, task, GetCurrentProcessId());

        //set timeout for recv
        if(setsockopt(connections[curThread].connec[task], SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(DWORD)) != 0 ){
            printf("Err setting timeout: %d\n", WSAGetLastError());
            if(endTask(curThread, task) != 0){
                printf("Err closing %d task of %d Thread: %d\n", task, curThread, WSAGetLastError());
            }
        }

        int recvV = recv(connections[curThread].connec[task], request, max_request_header_size, 0);
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

            default:
                break;
        }

        if(res.status != 413){
            readHeader(request, &reqH, &res);     //read the header and put the data into reqH
        }
        int createURLres = createURL(&reqH);
        if(createURLres == 404){
            res.status = 404;
            resErr(&res);
        }
        getExt(reqH.file, &(reqH.ext));
        getInterpreter(&reqH);
        if(reqH.ifcgi == TRUE && res.bodyN == TRUE){
            execute_cgi(&reqH, &res);
        }
        if(res.bodyN == TRUE){
            readFile(&res, reqH.file);
        }
        if(res.fSize != 0 && res.mimeType == NULL){
            checkType(&reqH, &res);
        }
        response(&res);
        sendRes(res);

        free(reqH.ext);
        reqH.ext = NULL;
        free(reqH.file);
        reqH.file = NULL;

        if(res.fSize != 0){
            free(res.body);
            res.body = NULL;
        }
        free(res.mimeType);
        res.mimeType = NULL;
        free(res.header);
        res.header = NULL;

        ZeroMemory(request, max_request_header_size);

        if(endTask(curThread, task) != 0){
            printf("Err closing %d task of %d Thread: %d\n", task, curThread, WSAGetLastError());
        }
    }
    free(request);
    if(CloseHandle(connections[curThread].Thread) == FALSE){
        printf("Err closing %d thread: %d\n", curThread, GetLastError());
    }

    _endthreadex(0);
    return 0;
}

unsigned int __stdcall child_wait(void *arglist)
{
    DWORD parent_process_id = *(DWORD *)arglist;
    HANDLE parent_process = OpenProcess(PROCESS_ALL_ACCESS, TRUE, parent_process_id);
    if(WaitForSingleObject(parent_process, INFINITE) == WAIT_FAILED){
        printf("Err child waiting: %lu\n", GetLastError());
    }
    CloseHandle(parent_process);
    ExitProcess(0);
    return 0;
}

int child_main(int argc, char **argv)
{
    printf("starting child process: %lu\n", GetCurrentProcessId());
    int c_wID;
    DWORD parent_process_id = strtoul(argv[4], NULL, 10);
    HANDLE c_wH = (HANDLE)_beginthreadex(NULL, 0, child_wait, (void *)&parent_process_id, 0, &c_wID);
    int port = atoi(argv[2]);
    SOCKET listen_sock6 = strtoull(argv[3], NULL, 10);
    WSADATA wsa;
    int WSAerr = 0;
    WSAerr = WSAStartup(MAKEWORD(1, 1), &wsa);
    if(WSAerr != 0){
        printf("Err starting WSA: %d\n", WSAerr);
        return 1;
    }
    if(initTypes() == 1){
        printf("Err reading mime.types\n");
        return 1;
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
                child_main(argc, argv);
                return 1;
            }
        }

        argList[curThread] = curThread;
        connections[curThread].Thread = (HANDLE)_beginthreadex(NULL, 0, Thread, (void *)&argList[curThread], 0, &threadID);

        if(connections[curThread].Thread == 0){
            if(retryT < 2){
                printf("Err starting Nr. %d Thread\n", curThread);
                curThread--;
                retryT++;
                continue;
            }
            else{
                child_main(argc, argv);
                return 1;
            }
        }
    }

    int curThread = 0;
    SOCKET csock;
    SOCKADDR_STORAGE client;

    int len = sizeof(SOCKADDR_STORAGE);

    srand((unsigned)(time(NULL) + GetCurrentProcessId()));
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
    CloseHandle(c_wH);
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

int createChildProcess(int pID)
{
    DWORD parent_process_id = GetCurrentProcessId();

    ZeroMemory(&(m_pi[pID]), sizeof(PROCESS_INFORMATION));
    STARTUPINFO sui;
    sui.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&sui);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    int len = strlen(self) + 50;
    char *cmd = calloc(len, sizeof(char));
    snprintf(cmd, len, "%s -child %d %llu %lu", self, m_port, (unsigned long long)m_listen_sock6, parent_process_id);
    if(CreateProcessA(NULL, cmd, &sa, NULL, TRUE, 0, NULL, NULL, &sui, &(m_pi[pID])) == FALSE){
        free(cmd);
        return 1;
    }
    free(cmd);
    return 0;
}

void CALLBACK restart_Process(ULONG_PTR param)
{
    printf("Recreating Process: %llu\n", (unsigned long long)param);
    createChildProcess(param);
}

unsigned int __stdcall parent_wait(void *arglist)   //to check whether one of the processes is down
{
    int pID = *(int *)arglist;
    free(arglist);
    while(TRUE){
        WaitForSingleObject(m_pi[pID].hProcess, INFINITE);
        QueueUserAPC(restart_Process, parent_main_thread, pID);
        Sleep(1000);
    }
    return 0;
}

int parent_main(int argc, char **argv)
{
    printf("Enter a port: ");
    scanf_s("%d", &m_port);

    WSADATA wsa;
    if(WSAStartup(MAKEWORD(1, 1), &wsa) != 0){
        return 1;
    }

    m_listen_sock6 = socket(AF_INET6, SOCK_STREAM, 0);
    int optVal = 0;
    int optlen = sizeof(int);
    setsockopt(m_listen_sock6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&optVal, optlen);

    SOCKADDR_IN6 serverAddr6;
    ZeroMemory(&serverAddr6, sizeof(serverAddr6));

    serverAddr6.sin6_family = AF_INET6;
    serverAddr6.sin6_port = htons(m_port);
    serverAddr6.sin6_addr = in6addr_any;

    if(bind(m_listen_sock6, (struct sockaddr *)&serverAddr6, sizeof(serverAddr6)) == SOCKET_ERROR){
        printf("Err binding the socket: %d\n", WSAGetLastError());
        parent_main(argc, argv);
        return 1;
    }

    if(listen(m_listen_sock6, SOMAXCONN) == SOCKET_ERROR){
        printf("Err setting listen socket: %d\n", WSAGetLastError());
        parent_main(argc, argv);
        return 1;
    }

    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &parent_main_thread, DUPLICATE_SAME_ACCESS, TRUE, DUPLICATE_SAME_ACCESS);

    HANDLE parent_check_handle[number_process];

    for(int i = 0; i < number_process; i++){
        if(createChildProcess(i) == 1){
            printf("Err creating process: %d\n", GetLastError());
            continue;
        }
        int *argID = calloc(1, sizeof(int));
        *argID = i;
        int threadID = 0;
        parent_check_handle[i] = (HANDLE)_beginthreadex(NULL, 0, parent_wait, (void *)argID, 0, &threadID);
    }
    for( ; ; ){
        SleepEx(INFINITE, TRUE);
    }
    WSACleanup();
    for(int i = 0; i < number_process; i++){
        CloseHandle(m_pi[i].hProcess);
        CloseHandle(parent_check_handle[i]);
        TerminateProcess(m_pi[i].hProcess, 0);
    }
    return 0;
}

int main(int argc, char **argv)
{
    self = calloc(strlen(argv[0]) + 1, sizeof(char));
    strcpy(self, argv[0]);
    if(argc >= 4){
        if(strcmp(argv[1], "-child") == 0){
            child_main(argc, argv);
        }
    }
    else{
        parent_main(argc, argv);
    }
    free(self);
    return 0;
}
