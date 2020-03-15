/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");
    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
    /*---------HANDLING COMMANDS-----------*/
    char *command_in = strtok(buf, " ");  

    //List all files in server directory
    if(strcmp(command_in, "ls\n") == 0){
      command_in[strlen(command_in) - 1] = '\0';
      FILE *ls_open = popen(buf, "r");
      char p; 
      bzero(buf, BUFSIZE);
      while((p=fgetc(ls_open)) != EOF){
        int len = strlen(buf);
        buf[len] = p;
      }
    //Put file in server directory
    } else if (strcmp(command_in, "put") == 0){
      char *file_n = strtok(buf[strlen(command_in)], " ");
      file_n[strlen(file_n)-1] = '\0';
      FILE *fp = fopen(file_n, "wb");
      if(fp == NULL){
         bzero(buf, BUFSIZE);
         strcpy(buf, "File could not create correctly");
      } else {
        //Same as get on client side, receiving as long as the buffer is being filled
        //as long as the server has no received the EOF marker
         int successfully_written = 0; 
         do {
          bzero(buf, BUFSIZE);
          n = recvfrom(sockfd, buf, BUFSIZE, 0,(struct sockaddr *) &clientaddr, &clientlen);
            //Check if file is over with EOF marker
          if(buf[n - 1] == EOF) {
             n = n - 1;
          }
          successfully_written = fwrite(buf, 1, n, fp);
         } while (successfully_written == BUFSIZE);
          fclose(fp);
          bzero(buf, BUFSIZE);
          strcpy(buf, "File successfully written");
      }      

    //Delete file from server directory
    } else if (strcmp(command_in, "delete") == 0){
      //Parse out file name
      char *file_n = strtok(buf[strlen(command_in)], " ");
      file_n[strlen(file_n)-1] = '\0';
      FILE *fp = fopen(file_n, "r");
      //Check if the file exists
      if(fp == NULL){
        bzero(buf, BUFSIZE);
        strcpy(buf, "File does not exist in server directory\n");
      } else {
        //If it does, execute remove() on file and check if successful value is returned
        printf("FILE EXISTS\n");
        fclose(fp);
        int file_deleted = remove(file_n);
        if(file_deleted == 0){
          strcpy(buf, "File Deleted!");
        } else {
          strcpy(buf, "File NOT Deleted!");
        }
      }

    } else if (strcmp(command_in, "get") == 0){
      //Parse out file name
      char *file_n = strtok(buf[strlen(command_in)], " ");
      file_n[strlen(file_n) - 1] = '\0';
      FILE *fp = fopen(file_n, "rb");
      if(fp == NULL){
        printf("File doesn't exist in server directory\n");
      } else {
          //If it exists then send file to client side
          int successfully_read = 0;
          do { 
            bzero(buf, BUFSIZE);
            successfully_read = fread(buf, 1, BUFSIZE, fp);
            sendto(sockfd, buf, successfully_read, 0, (struct sockaddr *) &clientaddr, clientlen);
          } while(successfully_read == BUFSIZE);
          fclose(fp);
          char *str = "File successfully read";
          strcpy(buf, str);
      }

    //If command doesn't fit any if statements - command unknown - sent to client
    }else {
      bzero(buf, BUFSIZE);
      char *str = "Command not formatted correctly or is unknown.\n";
      strcpy(buf, str);
    }


    /* 
     * sendto: echo the input back to the client 
     */
    n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
  }
}
