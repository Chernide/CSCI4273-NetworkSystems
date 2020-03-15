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

#define MAXLINE 8192

void parse_conf(char *file_name, int *port, char **username, char **password){
	FILE *fp = fopen(file_name, "r");
	if(fp == NULL){
		printf("Client Conf: Failed to Open\n");
	}
	char *line;
	size_t len = 0;
	for(int i = 0; i < 4; i++){
		getline(&line, &len, fp);
		strtok(line, ":");
		char *p_ = strtok(NULL, "\n");
		int p = atoi(p_);
		port[i] = p;
	}
	char *line2;
	getline(&line2, &len, fp);
	strtok(line2, " ");
	char *u_name = strtok(NULL, "\n");
	*username = u_name;

	char *line3;
	getline(&line3, &len, fp);
	strtok(line3, " ");
	char *pwd = strtok(NULL, "");
	*password = pwd;
	fclose(fp);
}

int open_socket(int portnum){
	int sockfd, optval = 1; 
	struct sockaddr_in serveraddr; 

	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		return -1;
	}

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0){
		return -1; 
	}
       

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)portnum); 

    if(connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0){
    	return -1; 
    }

    return sockfd;
}

char *str2md5(const char *str, int length) {
  int n;
  MD5_CTX c;
  unsigned char digest[16];
  char *out = (char*)malloc(33);
  MD5_Init(&c);
  while (length > 0) {
      if (length > 512) {
          MD5_Update(&c, str, 512);
      } else {
          MD5_Update(&c, str, length);
      }
      length -= 512;
      str += 512;
  }
  MD5_Final(digest, &c);
  for (n = 0; n < 16; ++n) {
      snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
  }

  return out;
}

void List(int *sockfds){
	char *buf[MAXLINE];
	char files[100][100];
	char uniq[100][100];
	int complete[100][4];

	for(int i = 0; i < 100; i++){
		files[i][0] = '\0';
		uniq[i][0] = '\0';
		complete[i][0] = -1;
		complete[i][1] = -1;
		complete[i][2] = -1;
		complete[i][3] = -1;
	}
	int files_idx = 0;
	for(int i = 0; i < 4; i++){
		if(sockfds[i] != -1){
			bzero(buf, MAXLINE);
			read(sockfds[i], buf, MAXLINE);
			while(strcmp(buf, "DONE") != NULL){
				strcpy(files[files_idx], buf);
				memmove(files[files_idx], files[files_idx] + 1, strlen(files[files_idx]));
				files_idx++;
				bzero(buf, MAXLINE);
				read(sockfds[i], buf, MAXLINE);
			}
		}
	}

	int tis_there = 0;
	int uniq_count = 0;
	for(int i = 0; i < 100; i++){
		if(files[i][0] != '\0'){
			tis_there = 0;
			for(int j = 0; j < 100; j++){
				if(uniq[j][0] != '\0'){
					char num = files[i][strlen(files[i]) - 1];
					files[i][strlen(files[i]) - 1] = '\0';
					if(strcmp(files[i], uniq[j]) == NULL){
						tis_there = 1;
					}
					files[i][strlen(files[i])] = num;
					files[i][strlen(files[i])] = '\0';
				} else {
					break;
				}
			}
			if(!tis_there){
				strcpy(uniq[uniq_count], files[i]);
				uniq[uniq_count][strlen(uniq[uniq_count]) - 1] = '\0';
				uniq_count++;
			}
		} else {
			break;
		}
	}

	for(int i = 0; i < 100; i++){
		if(files[i][0] != '\0'){
			tis_there = 0;
			for(int j = 0; j < 100; j++){
				if(uniq[j][0] != '\0'){
					char num = files[i][strlen(files[i]) - 1];
					files[i][strlen(files[i]) - 1] = '\0';
					if(strcmp(files[i], uniq[j]) == NULL){
						if(num == '1'){
							complete[j][0] = 1;
						} else if(num == '2'){
							complete[j][1] = 1;
						} else if(num == '3'){
							complete[j][2] = 1;
						} else if(num == '4'){
							complete[j][3] = 1;
						}
					}
					files[i][strlen(files[i])] = num;
					files[i][strlen(files[i])] = '\0';
				} else {
					break;
				}
			}
		} else {
			break;
		}
	}

	for(int i = 0; i < 100; i++){
		if(uniq[i][0] != '\0'){
			int sum = 0;
			for(int j = 0; j < 4; j++){
				sum += complete[i][j];
			}
			uniq[i][strlen(uniq[i]) - 1] = '\0';
			if(sum == 4){
				printf("%s\n", uniq[i]);
			} else {
				printf("%s [incomplete]\n", uniq[i]);
			}
		} else {
			break;
		}
	}


}

int main(int argc, char **argv){
	if(argc != 2){
		fprintf(stderr, "usage: %s <configuation file>\n", argv[0]);
		exit(0);
	}
	char *conf_file = argv[1];

	int port[4];
	char *username; 
	char *password;
	parse_conf(conf_file, port, &username, &password);

	int sockfds[4];
	for(int i = 0; i < 4; i++){
		if((sockfds[i] = open_socket(port[i])) == -1){
			printf("Error opening socket on port %d\n", port[i]);
		} else {
			printf("Socket connected on port %d\n", port[i]);
		}
	}

	char *buf[MAXLINE];
	sprintf(buf, "%s,%s", username, password);
	int cred = -1; 
	for(int i = 0; i < 4; i++){
		if(sockfds[i] != -1){
			bzero(buf, MAXLINE);
			sprintf(buf, "%s,%s", username, password);
			write(sockfds[i], buf, MAXLINE);
			bzero(buf, MAXLINE);
			read(sockfds[i], buf, MAXLINE);
			if(strcmp("-1", buf) == NULL){
				printf("Invalid Username/Password. Please try again.\n");
			} else if(strcmp("1", buf) == NULL){
				cred = 1;
			}
		}
	}
	if(cred == 1){
		bzero(buf, MAXLINE);
		sprintf(buf, "%s", username);
		for(int i = 0; i < 4; i++){
			if(sockfds[i] != -1){
				write(sockfds[i], buf, MAXLINE);
			}
		}
		char *choice[200];
		while(1){
			printf("\n1. LIST\n2. PUT <filename>\n3. GET <filename>\n> ");
			fgets(choice, 200, stdin);
			if(strstr(choice, "LIST") != NULL){
				strcpy(buf, "LIST");
				for(int i = 0; i < 4; i++){
					if(sockfds[i] != -1){
						write(sockfds[i], buf, MAXLINE);
					}
				}
				List(sockfds);
			} else if(strstr(choice, "PUT") != NULL){
				printf("PUT COMMAND\n");
				char *tmp = strtok(choice, " ");
				char *file_name = strtok(NULL, "\n");
				if(file_name != NULL){
					bzero(buf, MAXLINE);
					strcpy(buf, "PUT");
					for(int i = 0; i < 4; i++){
						write(sockfds[i], buf, MAXLINE);
					}
					bzero(buf, MAXLINE);
					strcpy(buf, file_name);
					for(int i = 0; i < 4; i++){
						write(sockfds[i], buf, MAXLINE);
					}
					FILE *fp = fopen(file_name, "rb");
					if(fp == NULL){
						printf("FILE INVALID\n");
					} else {
						fseek(fp, 0, SEEK_END); 
						int fs_ind = ftell(fp); 
						fseek(fp, 0, SEEK_SET); 
						char *hash_buf[MAXLINE];
						fread(hash_buf, 1, MAXLINE, fp);
						int hash = atoi(str2md5(hash_buf, strlen(hash_buf))) % 4;
						printf("HASH IS: %d\n", hash);
						int piece_size = fs_ind/4; 
						int extra = fs_ind % 4;

						fseek(fp, 0, SEEK_SET);
						char *piece1[MAXLINE];
						fread(piece1, 1, piece_size, fp);

						char *piece2[MAXLINE];
						fread(piece2, 1, piece_size, fp);

						char *piece3[MAXLINE];
						fread(piece3, 1, piece_size, fp);
						
						char *piece4[MAXLINE];
						fread(piece4, 1, piece_size + extra, fp);

						fclose(fp);
						if(hash == 0){
							if(sockfds[0] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[0], buf, MAXLINE);
								write(sockfds[0], piece1, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "1");
								write(sockfds[0], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[0], buf, MAXLINE);
								write(sockfds[0], piece2, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "2");
								write(sockfds[0], buf, 1);

							}
							if(sockfds[1] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[1], buf, MAXLINE);
								write(sockfds[1], piece2, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "2");
								write(sockfds[1], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[1], buf, MAXLINE);
								write(sockfds[1], piece3, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "3");
								write(sockfds[1], buf, 1);
							}
							if(sockfds[2] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[2], buf, MAXLINE);
								write(sockfds[2], piece3, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "3");
								write(sockfds[2], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size + extra);
								write(sockfds[2], buf, MAXLINE);
								write(sockfds[2], piece4, piece_size + extra);
								bzero(buf, MAXLINE);
								strcpy(buf, "4");
								write(sockfds[2], buf, 1);
							}
							if(sockfds[3] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size + extra);
								write(sockfds[3], buf, MAXLINE);
								write(sockfds[3], piece4, piece_size + extra);
								bzero(buf, MAXLINE);
								strcpy(buf, "4");
								write(sockfds[3], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[3], buf, MAXLINE);
								write(sockfds[3], piece1, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "1");
								write(sockfds[3], buf, 1);
							}
						} else if(hash == 1){
							if(sockfds[0] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size + extra);
								write(sockfds[0], buf, MAXLINE);
								write(sockfds[0], piece4, piece_size + extra);
								bzero(buf, MAXLINE);
								strcpy(buf, "4");
								write(sockfds[0], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[0], buf, MAXLINE);
								write(sockfds[0], piece1, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "1");
								write(sockfds[0], buf, 1);

							}
							if(sockfds[1] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[1], buf, MAXLINE);
								write(sockfds[1], piece1, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "1");
								write(sockfds[1], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[1], buf, MAXLINE);
								write(sockfds[1], piece2, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "2");
								write(sockfds[1], buf, 1);
							}
							if(sockfds[2] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[2], buf, MAXLINE);
								write(sockfds[2], piece2, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "2");
								write(sockfds[2], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[2], buf, MAXLINE);
								write(sockfds[2], piece3, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "3");
								write(sockfds[2], buf, 1);
							}
							if(sockfds[3] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[3], buf, MAXLINE);
								write(sockfds[3], piece3, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "3");
								write(sockfds[3], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size + extra);
								write(sockfds[3], buf, MAXLINE);
								write(sockfds[3], piece4, piece_size + extra);
								bzero(buf, MAXLINE);
								strcpy(buf, "4");
								write(sockfds[3], buf, 1);
							}
						} else if(hash == 2){
							if(sockfds[0] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[0], buf, MAXLINE);
								write(sockfds[0], piece3, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "3");
								write(sockfds[0], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size + extra);
								write(sockfds[0], buf, MAXLINE);
								write(sockfds[0], piece4, piece_size + extra);
								bzero(buf, MAXLINE);
								strcpy(buf, "4");
								write(sockfds[0], buf, 1);

							}
							if(sockfds[1] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size + extra);
								write(sockfds[1], buf, MAXLINE);
								write(sockfds[1], piece4, piece_size + extra);
								bzero(buf, MAXLINE);
								strcpy(buf, "4");
								write(sockfds[1], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[1], buf, MAXLINE);
								write(sockfds[1], piece1, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "1");
								write(sockfds[1], buf, 1);
							}
							if(sockfds[2] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[2], buf, MAXLINE);
								write(sockfds[2], piece1, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "1");
								write(sockfds[2], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[2], buf, MAXLINE);
								write(sockfds[2], piece2, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "2");
								write(sockfds[2], buf, 1);
							}
							if(sockfds[3] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[3], buf, MAXLINE);
								write(sockfds[3], piece2, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "2");
								write(sockfds[3], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[3], buf, MAXLINE);
								write(sockfds[3], piece3, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "3");
								write(sockfds[3], buf, 1);
							}
						} else if(hash == 3){
							if(sockfds[0] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[0], buf, MAXLINE);
								write(sockfds[0], piece2, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "2");
								write(sockfds[0], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[0], buf, MAXLINE);
								write(sockfds[0], piece3, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "3");
								write(sockfds[0], buf, 1);

							}
							if(sockfds[1] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[1], buf, MAXLINE);
								write(sockfds[1], piece3, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "3");
								write(sockfds[1], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size + extra);
								write(sockfds[1], buf, MAXLINE);
								write(sockfds[1], piece4, piece_size + extra);
								bzero(buf, MAXLINE);
								strcpy(buf, "4");
								write(sockfds[1], buf, 1);
							}
							if(sockfds[2] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size + extra);
								write(sockfds[2], buf, MAXLINE);
								write(sockfds[2], piece4, piece_size + extra);
								bzero(buf, MAXLINE);
								strcpy(buf, "4");
								write(sockfds[2], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[2], buf, MAXLINE);
								write(sockfds[2], piece1, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "1");
								write(sockfds[2], buf, 1);
							}
							if(sockfds[3] != -1){
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[3], buf, MAXLINE);
								write(sockfds[3], piece1, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "1");
								write(sockfds[3], buf, 1);
								bzero(buf, MAXLINE);
								sprintf(buf, "%d", piece_size);
								write(sockfds[3], buf, MAXLINE);
								write(sockfds[3], piece2, piece_size);
								bzero(buf, MAXLINE);
								strcpy(buf, "2");
								write(sockfds[3], buf, 1);
							}
						}
					}

				} else {
					printf("usage: PUT <file_name>\n");
				}
			} else if(strstr(choice, "GET") != NULL){
				printf("GET COMMAND\n");
				char *tmp = strtok(choice, " ");
				char *file_name = strtok(NULL, "\n");
				if(file_name != NULL){
					printf("HEre1\n");
					bzero(buf, MAXLINE);
					strcpy(buf, "GET");
					for(int i = 0; i < 4; i++){
						if(sockfds[i] != -1){
							write(sockfds[i], buf, MAXLINE);
						}
					}
					printf("HEre2\n");

					bzero(buf, MAXLINE);
					strcpy(buf, file_name);
					for(int i = 0; i < 4; i++){
						if(sockfds[i] != -1){
							int size = strlen(buf);
							write(sockfds[i], (char *)&size, sizeof(size));
							write(sockfds[i], buf, size);
						}
					}
					printf("HEre3\n");

					int complete[4];
					for(int i = 0; i < 4; i++){
						complete[i] = -1;
					}
					printf("HEre4\n");

					for(int i = 0; i < 4; i++){
						if(sockfds[i] != -1){
							bzero(buf, MAXLINE);
							int size;
							read(sockfds[i], (char*)&size, sizeof(size));
							read(sockfds[i], buf, size);
							while(strcmp(buf, "DONE") != NULL){
								complete[atoi(buf) - 1] = 1;
								bzero(buf, MAXLINE);
								read(sockfds[i], (char*)&size, sizeof(size));
								read(sockfds[i], buf, size);
							}
						}
					}
					int sum = 0;
					for(int i = 0; i < 4; i++){
						sum += complete[i];
					}
					printf("SUM: %d\n", sum);
					int size;
					if(sum == 4){
						printf("Commence file retrieval\n");
						char *part_names[4][50];
						sprintf(part_names[0], ".%s.1", file_name);
						sprintf(part_names[1], ".%s.2", file_name);
						sprintf(part_names[2], ".%s.3", file_name);
						sprintf(part_names[3], ".%s.4", file_name);

						bzero(buf, MAXLINE);
						strcpy(buf, "YES");
						size = strlen(buf);
						for(int i = 0; i < 4; i++){
							if(sockfds[i] != -1){
								write(sockfds[i], (char *)&size, sizeof(size));
								write(sockfds[i], buf, size);
							}
						}
						char *fn[25];
						sprintf(fn, "./Gotten_files/%s", file_name);
						int got[4];
						for(int i = 0; i < 4; i++){
							got[i] = -1;
						}
						FILE *out = fopen(fn, "ab+");
						for(int i = 0; i < 4; i++){
							for(int j = 0; j < 4; j++){
								if(sockfds[j] != -1){
									if(got[i] == 1){
										bzero(buf, MAXLINE);
										strcpy(buf, "DN");
										size = strlen(buf);
										write(sockfds[j], (char *)&size, sizeof(size));
										write(sockfds[j], buf, size);
										continue;
									} else {
										bzero(buf, MAXLINE);
										strcpy(buf, "NEED");
										size = strlen(buf);
										write(sockfds[j], (char *)&size, sizeof(size));
										write(sockfds[j], buf, size);
									}
									bzero(buf, MAXLINE);
									strcpy(buf, part_names[i]);
									printf("ASKING FOR PART: %s\n", buf);
									
									size = strlen(buf);
									write(sockfds[j], (char *)&size, sizeof(size));
									write(sockfds[j], buf, size);

									bzero(buf, MAXLINE);
									read(sockfds[j], (char*)&size, sizeof(size));
									read(sockfds[j], buf, size);
									printf("READ IN: %s\n", buf);
									if(strcmp(buf, "NO") != NULL){
										got[i] = 1;
										printf("BEFORE\n");
										fwrite(buf, 1, size, out);
										printf("AFTER W\n");
									} else {
										printf("THEY AINT GOTS IT\n");
									}
								}
								printf("======Next server======\n");

							}
						}
						fclose(out);
					} else {
						printf("File is incomplete.\n");	
						bzero(buf, MAXLINE);
						strcpy(buf, "NO");
						size = strlen(buf);
						for(int i = 0; i < 4; i++){
							if(sockfds[i] != -1){
								write(sockfds[i], (char *)&size, sizeof(size));
								write(sockfds[i], buf, size);
							}
						}
					}
				} else {
					printf("usage: GET <file_name>\n");
				}

			}
		}
	}

}