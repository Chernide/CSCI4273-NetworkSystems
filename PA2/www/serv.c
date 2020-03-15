/* 
 * tcpechosrv.c - A concurrent TCP echo server using threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

int open_listenfd(int port);
int write_packet(char *http_type, int size, FILE *fp, int connfd, char *content_type);
void echo(int connfd);
void *thread(void *vargp);

int main(int argc, char **argv) 
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (1) {
	connfdp = malloc(sizeof(int));
	*connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
	pthread_create(&tid, NULL, thread, connfdp);
    }
}

int write_packet(char *http_type, int fs, FILE *fp, int connfd, char *content_type){
    char buf[MAXLINE]; 
    char httpmsg[MAXLINE];
    bzero(httpmsg, MAXLINE);
    sprintf(httpmsg, "%s %s%s\r\n%s%d\r\n\r\n", http_type, " 200 Document Follows\r\nContent-Type: ", content_type,  "Content-Length:", fs);
    strcpy(buf, httpmsg);
    write(connfd, buf, strlen(httpmsg));
    char *fc[MAXLINE];
    int written = 0;
    do{
        bzero(fc, MAXLINE);
        bzero(httpmsg, MAXLINE);
        bzero(buf, MAXLINE);
        int rs = fread(fc, 1, MAXLINE, fp);
        fs = fs - rs; 
        // strcpy(httpmsg, fc);
        // strcpy(buf,httpmsg);
        written += write(connfd, fc, rs);
    }while(fs > MAXLINE);

    if(fs > 0){
        bzero(fc, MAXLINE);
        bzero(httpmsg, MAXLINE);
        bzero(buf, MAXLINE);
        int rs = fread(fc, 1, fs, fp);
        fs = fs - rs;
        // strcpy(httpmsg, fc);
        // strcpy(buf, httpmsg);
        written += write(connfd, fc, rs);
    }

    return written;
}


/* thread routine */
void * thread(void * vargp) 
{  
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self()); 
    free(vargp);
    echo(connfd);
    close(connfd);
    return NULL;
}

/*
 * echo - read and echo text lines until client closes connection
 */
void echo(int connfd) 
{
    size_t n; 
    char buf[MAXLINE]; 
    char httpmsg[MAXLINE];//="HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:37\r\n\r\n<html><h1>Hello CSCI4273 Course!</h1>"; 

    n = read(connfd, buf, MAXLINE);
    printf("server received the following request:\n%s\n",buf);
    //PARSE BUF HERE and GENERATE HTTPMSG
    char *get = strtok(buf, "\n");
   
    char *input =  strtok(get, " ");
    input = strtok(NULL, " ");
    char *request = input;
    input = strtok(NULL, " ");
    char *type = input;
    if(request != NULL){
        if((strcmp(request, "/") == 0) || (strcmp(request, "/inside/") == 0)){
            FILE *fp = fopen("index.html", "rb");
            if(fp != NULL){
                fseek(fp, 0, SEEK_END);
                int fs_ind = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                int ret1 = write_packet(type, fs_ind, fp, connfd, "text/html");
                printf("SUCCESSFUL BYTES WRITTEN: %d\n", ret1);
            } else { 
                char *new_message[250];
                strcpy(new_message, type);
                char *tmp[131]; 
                strcpy(tmp, "500 Internal Server Error\r\nContent-Type:text/html\r\nContent-Length:49\r\n\r\n<html><h3>An internal server has occurred :(</h3>");
                strcat(new_message, tmp);
                strcpy(httpmsg, new_message); 
                strcpy(buf,httpmsg);
                write(connfd, buf,strlen(httpmsg));
            }
        } else {
            memmove(request, request+1, strlen(request)); 
            if(strstr(request, "inside/") != NULL){
                memmove(request, request+7, strlen(request));   
                printf("%s\n", request);
            }
            printf("TYPE IS: %s\n", type);
            printf("INPUT IS: %s\n", request);
            FILE *fp = fopen(request, "rb");
            if(fp == NULL){
                printf("\nFile does not exist\n");
                char *new_message[250];
                strcpy(new_message, type);
                char *tmp[131]; 
                strcpy(tmp, "500 Internal Server Error\r\nContent-Type:text/html\r\nContent-Length:49\r\n\r\n<html><h3>An internal server has occurred :(</h3>");
                strcat(new_message, tmp);
                strcpy(httpmsg, new_message); 
                strcpy(buf,httpmsg);
                write(connfd, buf,strlen(httpmsg));
            } else { 
                char *content_type[40];
                if(strstr(request, ".html") != NULL){
                    strcpy(content_type, "text/html");
                } else if(strstr(request, ".txt") != NULL){
                    strcpy(content_type, "text/plain");
                } else if(strstr(request, ".png") != NULL){
                    strcpy(content_type, "image/png");
                } else if(strstr(request, ".gif") != NULL){
                    strcpy(content_type, "image/gif");
                } else if(strstr(request, ".jpg") != NULL){
                    strcpy(content_type, "image/jpg");
                } else if(strstr(request, ".css") != NULL){
                    strcpy(content_type, "text/css");
                } else if(strstr(request, ".js") != NULL){
                    strcpy(content_type, "application/javascript");
                }

                printf("CONTENT TYPE IS: %s\n", content_type);

                fseek(fp, 0, SEEK_END);
                int fs = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                printf("FILE SIZE IS: %d\n", fs);
                int ret = write_packet(type, fs, fp, connfd, content_type);
                printf("SUCCESSFUL BYTES WRITTEN: %d\n", ret);
                fclose(fp);
            }
        }
    }
   
    
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */
