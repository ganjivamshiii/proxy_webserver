#include "proxy_parse.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif
// Windows doesn't have bzero and bcopy, so we define them
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#define bcopy(src,dest,len) (memmove(dest, src, len))
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <strings.h>  // For bzero and bcopy on Unix systems
#endif


#define MAX_CLIENTS 10
#define MAX_BYTES 4096
#define MAX_SIZE  200*(1<<20) 
#define MAX_ELEMENT_SIZE 10*(1<<20)

typedef struct cache_element cache_element;

struct cache_element{
    char* data;
    int len;
    char* url;
    time_t lru_time_track;
    cache_element* next;
};

cache_element* find(char* url);
int add_cache_element(char* data,int size, char* url);
void remove_cache_element();

int port_number=8081;
int proxy_socketId;
pthread_t tid[MAX_CLIENTS];

sem_t semaphore;
pthread_mutex_t lock;

cache_element* head;
int cache_size;

int sendErrorMessage(int socket, int status_code)
{
    char str[1024];
    char currentTime[50];
    time_t now = time(0);

    struct tm data = *gmtime(&now);
    strftime(currentTime,sizeof(currentTime),"%a, %d %b %Y %H:%M:%S %Z", &data);

    switch(status_code)
    {
        case 400: snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\n</BODY></HTML>", currentTime);
                  printf("400 Bad Request\n");
                  send(socket, str, strlen(str), 0);
                  break;

        case 403: snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
                  printf("403 Forbidden\n");
                  send(socket, str, strlen(str), 0);
                  break;

        case 404: snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
                  printf("404 Not Found\n");
                  send(socket, str, strlen(str), 0);
                  break;

        case 500: snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
                  send(socket, str, strlen(str), 0);
                  break;

        case 501: snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
                  printf("501 Not Implemented\n");
                  send(socket, str, strlen(str), 0);
                  break;

        case 505: snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
                  printf("505 HTTP Version Not Supported\n");
                  send(socket, str, strlen(str), 0);
                  break;

        default:  return -1;
    }
    return 1;
}

int connectRemoteServer(char* host_addr,int port_num){
    int remoteSocket=socket(AF_INET,SOCK_STREAM,0);
    if(remoteSocket<0){
        printf("Error in creating your socket\n");
        return -1;
    }
    struct hostent* host=gethostbyname(host_addr);
    if(host==NULL){
        fprintf(stderr,"No such host exist\n");
        return -1;
    }
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port_num);
    bcopy((char *)host->h_addr,(char *)&server_addr.sin_addr.s_addr,host->h_length);
    if(connect(remoteSocket,(struct sockaddr *)&server_addr,(socklen_t)sizeof(server_addr)) < 0){
       fprintf(stderr,"Error in connecting\n"); // Fixed: frprintf -> fprintf
       return -1;
    }
    else{
        return remoteSocket;
    }
}

int handle_request(int clientSocketId, ParsedRequest* request, char *tempReq){
    char *buf=(char *) malloc(sizeof(char)*MAX_BYTES);
    strcpy(buf,"GET ");
    strcat(buf,request->path);
    strcat(buf," ");
    strcat(buf,request->version);
    strcat(buf,"\r\n");
    size_t len=strlen(buf);
    if(ParsedHeader_get(request,"Host")==NULL){
        if(ParsedHeader_set(request, "Host",request->host) < 0){ // Fixed: removed <0 comparison
            printf("set host header key is not working");
        }
    }
    if(ParsedRequest_unparse_headers(request,buf+len,(size_t)MAX_BYTES-len)<0){
        printf("unparser failed");
    }
    int server_port=80;
    if(request->port!=NULL){
        server_port=atoi(request->port);
    }
    int remoteSocketId=connectRemoteServer(request->host,server_port);
    if(remoteSocketId<0){
        return -1;
    }
    int bytes_send=send(remoteSocketId,buf,strlen(buf),0);
    bzero(buf,MAX_BYTES);
    bytes_send=recv(remoteSocketId,buf,MAX_BYTES-1,0);
    char *temp_buffer=(char*)malloc(sizeof(char)*MAX_BYTES);
    int temp_buffer_size=MAX_BYTES;
    int temp_buffer_index=0;
    while(bytes_send>0){
        bytes_send=send(clientSocketId,buf,bytes_send,0);
        for(int i=0;i<bytes_send/sizeof(char);i++){
            temp_buffer[temp_buffer_index]=buf[i];
            temp_buffer_index++;
        }
        temp_buffer_size+=MAX_BYTES;
        temp_buffer=(char*)realloc(temp_buffer,temp_buffer_size);
        if(bytes_send<0){
            perror("error in sending data to the client\n");
            break;
        }
        bzero(buf, MAX_BYTES);
        bytes_send=recv(remoteSocketId,buf,MAX_BYTES-1,0);
    }
    temp_buffer[temp_buffer_index]='\0';
    free(buf);
    add_cache_element(temp_buffer,strlen(temp_buffer),tempReq);
    free(temp_buffer);
    close(remoteSocketId);
    return 0;
}

int checkHTTPversion(char *msg)
{
    int version=-1;

    if(strncmp(msg,"HTTP/1.1",8)==0){
        version=1;
    }
    else if(strncmp(msg,"HTTP/1.0",8)==0){
        version=1;
    }
    else{
        version=-1;
    }
    return version;
}

void *thread_fn(void *socketNew){
    sem_wait(&semaphore);
    int p;
    sem_getvalue(&semaphore, &p); // Fixed: pass address of p
    printf("semaphore value is : %d\n",p);
    int socket = *((int*)socketNew); // Fixed: properly extract socket descriptor
    int bytes_send_client, len;
    
    char *buffer=(char*) calloc(MAX_BYTES,sizeof(char));
    bzero(buffer,MAX_BYTES);
    bytes_send_client=recv(socket,buffer, MAX_BYTES,0);

    while(bytes_send_client>0){
        len=strlen(buffer);
        if(strstr(buffer,"\r\n\r\n")==NULL){
            bytes_send_client=recv(socket,buffer+len,MAX_BYTES-len,0);
        }else{
            break;
        }   
    }
    char *tempReq=(char *)malloc(strlen(buffer)*sizeof(char)+1);
    for(int i=0;i<strlen(buffer);i++){
        tempReq[i]=buffer[i];
    }
    tempReq[strlen(buffer)] = '\0'; // Null terminate
    
    struct cache_element* temp=find(tempReq);
    if(temp!=NULL){
        int size=temp->len/sizeof(char);
        int pos=0;
        char response[MAX_BYTES];
        while(pos<size){
            bzero(response,MAX_BYTES);
            int remaining = size - pos;
            int to_copy = (remaining > MAX_BYTES) ? MAX_BYTES : remaining;
            memcpy(response, temp->data + pos, to_copy);
            send(socket,response,to_copy,0);
            pos += to_copy;
        }
        printf("data retrieved from cache\n");
    }else if(bytes_send_client>0){
        len=strlen(buffer);
        ParsedRequest *request= ParsedRequest_create();

        if(ParsedRequest_parse(request,buffer,len)<0){
           printf("parsing failed\n");
        }else{
            bzero(buffer,MAX_BYTES);
            if(!strcmp(request->method, "GET")){
               if(request->host && request->path && checkHTTPversion(request->version)==1){
                bytes_send_client=handle_request(socket,request, tempReq);
                if(bytes_send_client==-1){
                    sendErrorMessage(socket,500);
                }
                // Removed the else clause that was sending error on success
               }else{
                printf("this code doesn't support any method apart from GET");
               }
            }
            ParsedRequest_destroy(request);
        }
    }else if(bytes_send_client==0){
        printf("client is disconnected");
    }
    shutdown(socket,SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);
    sem_getvalue(&semaphore,&p); // Fixed: pass address of p
    printf("semaphore post value %d\n",p);
    free(tempReq);
    return NULL;
}


int main(int argc, char* argv[]) {
    int client_socketId;
    int client_len;
    struct sockaddr_in server_addr, client_addr;

#ifdef _WIN32
    WSADATA wsaData;
    int wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaerr != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", wsaerr);
        return 1;
    }
#endif

    sem_init(&semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    if (argc == 2) {
        port_number = atoi(argv[1]);
    } else {
        printf("Please enter one argument: <port>\n");
        exit(1);
    }

    printf("Starting proxy server at port :%d\n", port_number);
    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socketId < 0) {
#ifdef _WIN32
        fprintf(stderr, "Failed to create socket. WSA Error: %d\n", WSAGetLastError());
#else
        perror("Failed to create socket");
#endif
        exit(1);
    }

    int reuse = 1;
    setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Port is not available");
        exit(1);
    }

    printf("Binding on port %d\n", port_number);

    if (listen(proxy_socketId, MAX_CLIENTS) < 0) {
        perror("Error in listening");
        exit(1);
    }

    int i = 0;
    int Connected_socketId[MAX_CLIENTS];

    while (1) {
        bzero((char *)&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketId = accept(proxy_socketId, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
        if (client_socketId < 0) {
            printf("Not able to connect\n");
            exit(1);
        } else {
            Connected_socketId[i] = client_socketId;
        }

        struct sockaddr_in *client_pt = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client connected from IP %s, port %d\n", str, ntohs(client_addr.sin_port));

        pthread_create(&tid[i], NULL, thread_fn, (void *)&Connected_socketId[i]);
        i++;
    }

    close(proxy_socketId);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

cache_element *find(char* url){
    cache_element*site=NULL;
    int time_lock_val=pthread_mutex_lock(&lock);
    printf("Remove cache Lock acquired %d\n ",time_lock_val);
    if(head!=NULL){
        site=head;
        while(site!=NULL){
            if(!strcmp(site->url,url)){
                printf("LRU time track before: %ld\n",site->lru_time_track);
                printf("\n url found\n");
                site->lru_time_track=time(NULL);
                printf("LRU time track after %ld\n",site->lru_time_track);
                break;
            }
            site=site->next;
        }
    }
    else{
        printf("url not found\n");
    }
    time_lock_val=pthread_mutex_unlock(&lock);
    printf("lock is unlocked\n");
    return site;
}

void remove_cache_element(){
    cache_element * p;
    cache_element * q;
    cache_element * temp;
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove Cache Lock Acquired %d\n",temp_lock_val); 
    if( head != NULL) {
        for (q = head, p = head, temp =head ; q -> next != NULL; 
            q = q -> next) {
            if(( (q -> next) -> lru_time_track) < (temp -> lru_time_track)) {
                temp = q -> next;
                p = q;
            }
        }
        if(temp == head) { 
            head = head -> next;
        } else {
            p->next = temp->next;    
        }
        cache_size = cache_size - (temp -> len) - sizeof(cache_element) - 
        strlen(temp -> url) - 1;
        free(temp->data);           
        free(temp->url);
        free(temp);
    } 
    temp_lock_val = pthread_mutex_unlock(&lock);
    printf("Remove Cache Lock Unlocked %d\n",temp_lock_val); 
}

int add_cache_element(char* data,int size,char* url){
    int temp_lock_val = pthread_mutex_lock(&lock);
    printf("Add Cache Lock Acquired %d\n", temp_lock_val);
    int element_size=size+1+strlen(url)+sizeof(cache_element);
    if(element_size>MAX_ELEMENT_SIZE){
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add Cache Lock Unlocked %d\n", temp_lock_val);
        return 0;
    }
    else
    {   
        while(cache_size+element_size>MAX_SIZE){
            remove_cache_element();
        }
        cache_element* element = (cache_element*) malloc(sizeof(cache_element));
        element->data= (char*)malloc(size+1);
        strcpy(element->data,data); 
        element -> url = (char*)malloc(1+( strlen( url )*sizeof(char)  ));
        strcpy( element -> url, url );
        element->lru_time_track=time(NULL);
        element->next=head; 
        element->len=size;
        head=element;
        cache_size+=element_size;
        temp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add Cache Lock Unlocked %d\n", temp_lock_val);
        return 1;
    }
    return 0;
}

