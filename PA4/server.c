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
#include <openssl/md5.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define LISTENQ 1024
#define MAXLINE 8192

char *dir = NULL;

int open_socket(int port){
	int listenfd, optval=1;
	struct sockaddr_in serveraddr;

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1; 

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
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
}

void *thread(void *vargp){
	int connfd = *((int *) vargp);
	pthread_detach(pthread_self());
	free(vargp);
	server(connfd);
	close(connfd);
	return NULL;
}

int check_credentials(char *u, char *p){
	FILE *fp = fopen("dfs.conf", "r");
	if(fp == NULL){
		return -1;
	} 
	char *line; 
	size_t len; 
	while(getline(&line, &len, fp) != -1){
		char *u_i = strtok(line, " ");
		char *p_i = strtok(NULL, "\n");
		if(strcmp(u, u_i) == NULL && strcmp(p, p_i) == NULL){
			fclose(fp);
			return 1; 
		}
	}
	fclose(fp);
	return -1;
}

void server(int connfd){
	char *PP[MAXLINE];
	printf("Thread created and sent to server.\n");
	printf("The directory is: %s\n", dir);
	char *buf[MAXLINE];
	char *user = NULL;
	char *pass = NULL;


	//GET CREDENTIALS
	read(connfd, buf, MAXLINE);
	user = strtok(buf, ",");
	char *user_cpy[100];
	strcpy(user_cpy, user);
	pass = strtok(NULL, "");
	int ret = check_credentials(user, pass);

	bzero(buf, MAXLINE);
	if(ret > 0){
		strcpy(buf, "1");
		write(connfd, buf, MAXLINE);
	} else {
		strcpy(buf, "-1");
		write(connfd, buf, MAXLINE);
		exit(0);
	}

	char *path[100]; 
	bzero(buf, MAXLINE);
	//directory name
	read(connfd, buf, MAXLINE);
	char *dirname[MAXLINE];
	strcpy(dirname, buf);
	sprintf(path, ".%s/%s", dir, buf);
	strcpy(PP, path);
	printf("CHECKING PATH: %s\n", path);
	struct stat st = {0};
	if(stat(path, &st) == -1){
		printf("Doesn't exist\n");
		printf("path: %s\n", path);
		mkdir(path, 0777); 
	} else {
		printf("Exists\n");
	}

	while(1){
		//Waiting for the incoming command
		read(connfd, buf, MAXLINE);
		printf("Command received: %s\n", buf);
		if(strstr(buf, "LIST") != NULL){
			FILE *fp; 
			char *cmd[100];
			sprintf(cmd, "ls -a %s", path);
			fp = popen(cmd, "r");
			char path[100];
			while(fgets(path, 1000, fp) != NULL){
				if(strlen(path) > 4){
					path[strlen(path) - 1] = '\0';
					bzero(buf, MAXLINE);
					strcpy(buf, path);
					printf("sending: %s\n", buf);
					write(connfd, buf, MAXLINE);
				}
			}
			bzero(buf, MAXLINE);
			strcpy(buf, "DONE");
			write(connfd, buf, MAXLINE);
			pclose(fp);
		} else if(strstr(buf, "PUT") != NULL){
			//RECEIVE FILE NAME
			bzero(buf, MAXLINE);
			read(connfd, buf, MAXLINE);
			printf("File name received: %s\n", buf);
			char *input_file[100];
			strcpy(input_file, buf);

			for(int i = 0; i < 2; i++){
				//READ PIECE SIZE
				bzero(buf, MAXLINE);
				read(connfd, buf, MAXLINE);
				int piece_size = atoi(buf);

				//READ PIECE CONTENTS
				bzero(buf, MAXLINE);
				read(connfd, buf, piece_size);
				char *piece[MAXLINE];
				strcpy(piece, buf);

				//RECEIVE PIECE NUMBER
				bzero(buf, MAXLINE);
				read(connfd, buf, 1);
				int p = atoi(buf);
				
				//Create file path
				char *fn[100];
				sprintf(fn, "%s/.%s.%d", path,input_file, p);

				//OPEN FILE, WRITE, AND CLOSE
				FILE *out = fopen(fn, "ab+");
				fwrite(piece, 1, piece_size, out);
				fclose(out);
			}
			
		} else if(strstr(buf, "GET") != NULL){
			int size;
			bzero(buf, MAXLINE);
			read(connfd, (char*)&size, sizeof(size));
			read(connfd, buf, size);
			char *file[MAXLINE];
			strcpy(file, buf);
			FILE *fp; 
			char *cmd[100];
			sprintf(cmd, "ls -a %s", path);
			fp = popen(cmd, "r");
			char path[100];
			while(fgets(path, 1000, fp) != NULL){
				if(strstr(path, file) != NULL){
					char *tmp = strtok(path, ".");
					tmp = strtok(NULL, ".");
					tmp = strtok(NULL, "");
					size = strlen(tmp);
					write(connfd, (char *)&size, sizeof(size));
					write(connfd, tmp, size);
				}
			}
			pclose(fp);
			bzero(buf, MAXLINE);
			strcpy(buf, "DONE");
			size = strlen(buf);
			write(connfd, (char *)&size, sizeof(size));
			write(connfd, buf, size);
			
			bzero(buf, MAXLINE);
			read(connfd, (char*)&size, sizeof(size));
			read(connfd, buf, size);
			if(strcmp(buf, "YES") == NULL){
				for(int i = 0; i < 4; i++){
					bzero(buf, MAXLINE);
					read(connfd, (char*)&size, sizeof(size));
					read(connfd, buf, size);
					if(strcmp(buf, "NEED") == NULL){
						bzero(buf, MAXLINE);
						read(connfd, (char*)&size, sizeof(size));
						read(connfd, buf, size);
						printf("SHOUD BE FILE: %s\n", buf);
						char *fn[100];
						sprintf(fn, "%s/%s", PP,buf);
						printf("PATH: %s\n", fn);
						FILE *fp = fopen(fn , "rb");
						if(fp == NULL){
							bzero(buf, MAXLINE);
							strcpy(buf, "NO");
							size = strlen(buf);
							write(connfd, (char *)&size, sizeof(size));
							write(connfd, buf, size);
						} else {
							bzero(buf, MAXLINE);
							fread(buf, 1, MAXLINE, fp);
							size = strlen(buf);
							write(connfd, (char *)&size, sizeof(size));
							write(connfd, buf, size);
							fclose(fp);
						}
					}
				}

			} else {
				printf("Ending function\n");
			}

			// printf("Ready to receive sending filename.\n");
			// bzero(buf, MAXLINE);
			// read(connfd, (char*)&size, sizeof(size));
			// read(connfd, buf, size);
			// char *files[4][100];
			// for(int i = 0; i < 4; i++){
			// 	files[i][0] = '\0';
			// }
			// int files_ind = 0;
			// while(strcmp(buf, "DONE") != NULL){
			// 	strcpy(files[files_ind], buf);
			// 	files_ind++;
			// 	bzero(buf, MAXLINE);
			// 	read(connfd, (char*)&size, sizeof(size));
			// 	read(connfd, buf, size);
			// }
			// for(int i = 0; i < 4; i++){
			// 	if(files[i][0] != '\0'){
			// 		char *fn[100];
			// 		sprintf(fn, "%s/%s", PP, files[i]);
			// 		printf("File to open: %s\n", fn);
			// 		FILE *fp = fopen(fn, "rb");

			// 		bzero(buf, MAXLINE);
			// 		fread(buf, 1, MAXLINE, fp);
			// 		char *new_buf[MAXLINE * 2];
			// 		sprintf(new_buf, "%s/%s", files[i], buf);
			// 		size = strlen(new_buf);
			// 		write(connfd, (char *)&size, sizeof(size));
			// 		write(connfd, new_buf, size);
			// 		fclose(fp);
			// 	}
			// }
			// bzero(buf, MAXLINE);
			// strcpy(buf, "DONE");
			// size = strlen(buf);
			// write(connfd, (char *)&size, sizeof(size));
			// write(connfd, buf, size);
			// write(connfd, (char *)&size, sizeof(size));
			// write(connfd, buf, size);

			// printf("End of GET\n");
		}
	}
}

int main(int argc, char **argv){
	if(argc != 3){
		fprintf(stderr, "usage: %s <directory> <port> \n", argv[1]);
	}
	char *directory = argv[1]; 
	dir = directory;
	int port = atoi(argv[2]);
	pthread_t tid;
	int sockfd, *connfdp, clientlen=sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	sockfd = open_socket(port);

	while(1){
		connfdp = malloc(sizeof(int));
		*connfdp = accept(sockfd, (struct sockaddr*)&clientaddr, &clientlen);
		pthread_create(&tid, NULL, thread, connfdp);		
	}
}


