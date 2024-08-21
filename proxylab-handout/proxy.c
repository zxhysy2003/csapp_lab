#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define SBUFSIZE 16
#define NTHREADS 4

sbuf_t sbuf; // connection with buffer

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

struct Uri
{
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
};

void sigpipe_handler();
void doit(int connfd);
void build_header(char *http_header,struct Uri *uri_data,rio_t *client_rio);
void parse_uri(char *uri,struct Uri *uri_data);
void *thread(void *vargp);

int main(int argc,char **argv)
{
    int listenfd,connfd;
    socklen_t clientlen;
    char hostname[MAXLINE],port[MAXLINE];
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if(argc != 2) {
        fprintf(stderr,"usage: %s <port>\n",argv[0]);
        exit(1);
    }
    signal(SIGPIPE,sigpipe_handler);
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf,SBUFSIZE);

    // create worker thread
    for(int i = 0;i < NTHREADS;i++) {
        Pthread_create(&tid,NULL,thread,NULL);
    }

    while(1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd,(SA *)&clientaddr,&clientlen);

        // add fd to buffer
        sbuf_insert(&sbuf,connfd);

        Getnameinfo((SA *)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
        printf("Accepted connection from (%s %s)\n",hostname,port);

        //doit(connfd);

        //Close(connfd);
    }

    return 0;
}
void *thread(void *vargp)
{
    Pthread_detach(Pthread_self());
    while(1) {
        int connfd = sbuf_remove(&sbuf);
        doit(connfd);
        Close(connfd);
    }
}

void sigpipe_handler()
{
    printf("SIGPIPE DO NOTHING...\n");
    return;
}

void doit(int connfd)
{
    char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char server[MAXLINE];

    rio_t rio,server_rio;

    Rio_readinitb(&rio,connfd);
    Rio_readlineb(&rio,buf,MAXLINE);
    sscanf(buf,"%s %s %s",method,uri,version);

    if(strcasecmp(method,"GET")) {
        printf("Proxy does not implement the method");
        return;
    }

    struct Uri *uri_data = (struct Uri *)malloc(sizeof(struct Uri));
    parse_uri(uri,uri_data);

    build_header(server,uri_data,&rio);

    int serverfd = Open_clientfd(uri_data->host,uri_data->port);
    if(serverfd < 0) {
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&server_rio,serverfd);
    Rio_writen(serverfd,server,strlen(server));

    size_t n;
    while((n = Rio_readlineb(&server_rio,buf,MAXLINE)) != 0) {
        printf("proxy received %d bytes,then send.\n",(int)n);
        Rio_writen(connfd,buf,n);
    }

    Close(serverfd);

}

void parse_uri(char *uri,struct Uri *uri_data)
{
    char *hostpose = strstr(uri,"//");
    if(hostpose == NULL) {
        char *pathpose = strstr(uri,"/");
        if(pathpose != NULL) {
            strcpy(uri_data->path,pathpose);
        }
        strcpy(uri_data->port,"80");
        return;
    }
    else {
        char *portpose = strstr(hostpose + 2,":");
        if(portpose != NULL) {
            int tmp;
            sscanf(portpose + 1,"%d%s",&tmp,uri_data->path);
            sprintf(uri_data->port,"%d",tmp);
            *portpose = '\0';
        }
        else {
            char *pathpose = strstr(hostpose + 2,"/");
            if(pathpose != NULL) {
                strcpy(uri_data->path,pathpose);
                strcpy(uri_data->port,"80");
                *pathpose = '\0';
            }
        }
        strcpy(uri_data->host,hostpose + 2);
    }
    return;
}

void build_header(char *http_header,struct Uri *uri_data,rio_t *client_rio)
{
    char *conn_hdr = "Connection: close\r\n";
    char *prox_hdr = "Proxy-Connection: close\r\n";
    char *host_hdr_format = "Host: %s\r\n";
    char *request_hdr_format = "GET %s HTTP/1.0\r\n";
    char *endof_hdr = "\r\n";

    char buf[MAXLINE],request_hdr[MAXLINE],host_hdr[MAXLINE],other_hdr[MAXLINE];
    sprintf(request_hdr,request_hdr_format,uri_data->path);
    while(Rio_readlineb(client_rio,buf,MAXLINE) > 0)
    {
        if(strcmp(buf,endof_hdr) == 0)
            break;
        if(!strncasecmp(buf,"Host",strlen("Host"))) {
            strcpy(host_hdr,buf);
            continue;
        }
        if(strncasecmp(buf,"Connection",strlen("Connection")) && 
        strncasecmp(buf,"Proxy-Connection",strlen("Proxy-Connection")) &&
        strncasecmp(buf,"User-Agent",strlen("User-Agent"))) {
            strcat(other_hdr,buf);
        }
    }
    if(strlen(host_hdr) == 0)
    {
        sprintf(host_hdr,host_hdr_format,uri_data->host);
    }
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);

    return;
}

