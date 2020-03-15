/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
      (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    
    while(1) {
    		int rec = 1;

    		//Print Menu for user selection
        printf("-----MENU-----\n");
        printf("1) get [file_name]\n");
        printf("2) put [file_name]\n");
        printf("3) delete [file_name]\n");
        printf("4) ls\n");
        printf("5) exit\n");
        bzero(buf, BUFSIZE);
        printf("Enter command: ");


        //Save user input 
        fgets(buf, BUFSIZE, stdin);
        printf("\n");

        //Create second copy of command 
        //One copy for processing one for clean copy 
        char buf2[BUFSIZE];
        strcpy(buf2, buf);

        serverlen = sizeof(serveraddr);

        //Exit loop when "exit" is inputted
        if(strstr(buf, "exit") != NULL){
            break;
        } 
        //No processing needed if command is 'ls' or 'delete' on the client side
        //Thus we just send it 
        if((strstr(buf, "ls") !=NULL) || strstr(buf, "delete") != NULL){
          n = sendto(sockfd, buf2, strlen(buf2), 0, &serveraddr, serverlen);
          if (n < 0) {
            error("ERROR in sendto");
          }
        //If the command is put
        } else if(strstr(buf, "put") != NULL){
        		//Parse out file name 

            char *file_n = strtok(buf, " ");
            file_n = strtok(NULL, " ");
            if(file_n != NULL){
	            file_n[strlen(file_n)-1] = '\0';
	            FILE *fp = fopen(file_n, "rb");
	            //Check that file exists
	            if(fp == NULL){
	                printf("File does not exist in client directory\n");
	                rec = 0;
	            } else {
	            	 //If it does exist, now send command to server (using clean copy)
	               n = sendto(sockfd, buf2, strlen(buf2), 0, &serveraddr, serverlen);
	               if (n < 0) {
	                error("ERROR in sendto");
	               }
	               int successfully_read = 0; 
	               //While loop to loop through all information in file 
	               //Do while to send last remaining information after condition is false
	               do { 
	               	bzero(buf, BUFSIZE);
	               	successfully_read = fread(buf, 1, BUFSIZE, fp);
	               	sendto(sockfd, buf, successfully_read, 0, &serveraddr, serverlen);
	               } while(successfully_read == BUFSIZE);
	                fclose(fp);
	            }
	          } else {
	          	printf("No File Provided.\n");
	          	bzero(buf, BUFSIZE);
	          	strcpy(buf, "No File Prodivded");
	          	sendto(sockfd, buf, BUFSIZE, 0, &serveraddr, serverlen);

	          }
        //If the command is get
        } else if(strstr(buf, "get") != NULL){
        	//Send command to server side
        	n = sendto(sockfd, buf2, strlen(buf2), 0, &serveraddr, serverlen);
        	//Parse out the file name
          char *file_n = strtok(buf, " ");
    	    file_n = strtok(NULL, " ");
          file_n[strlen(file_n)-1] = '\0';
          FILE *fp = fopen(file_n, "wb");
          if(fp == NULL){
		         bzero(buf, BUFSIZE);
		         strcpy(buf, "File could not create correctly");
		      } else {
		         //Same do while loop as above, but receiving instead of sending
		         int successfully_written = 0; 
		         do {
		          bzero(buf, BUFSIZE);
		          n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
		          //Check if file is over with EOF marker
		          if(buf[n - 1] == EOF){
		          	n = n - 1;
		          }
		          successfully_written = fwrite(buf, 1, n, fp);
		         } while (successfully_written == BUFSIZE);
		          fclose(fp);
		          bzero(buf, BUFSIZE);
		          printf("File successfully written\n");
		      }      
        } else {
        	n = sendto(sockfd, buf2, strlen(buf2), 0, &serveraddr, serverlen);

        }

        bzero(buf, BUFSIZE);
        /* print the server's reply */
        if(rec == 1){
	        n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
	        if (n < 0) 
	          error("ERROR in recvfrom");
	        printf("Response from server:\n%s\n", buf);
        }
    }
    return 0;
}
