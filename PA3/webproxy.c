/*
 *  webproxy.c - A multithreaded web proxy for GET commands
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
#include <openssl/md5.h>

#define MAXLINE 8192 
#define LISTENQ 1024

pthread_mutex_t ip_lock;
pthread_mutex_t pg_lock;
pthread_mutex_t black_lock;

int open_listenfd(int port);
void *thread(void *vargp);

int main(int argc, char **argv){
	int sk1, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr; 
	pthread_t tid; 

	if(argc != 2){
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	port = atoi(argv[1]);

	sk1 = open_listenfd(port);
	while(1) { 
		connfdp = malloc(sizeof(int));
		*connfdp = accept(sk1, (struct sockaddr*)&clientaddr, &clientlen);
		pthread_create(&tid, NULL, thread, connfdp);
	}

}

void * thread(void * vargp){
	int connfd = *((int *) vargp);
	pthread_detach(pthread_self());
	free(vargp); 
	proxy(connfd);
	close(connfd);
	return NULL;
}

struct sockaddr_in check_hostname_cache(char *name, int s_port, int *valid){
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(s_port);
	int lookup = 1; 
	char *host;
	char *ip;
	struct hostent *h;
	char *mapping[100];
	pthread_mutex_lock(&ip_lock);
	FILE *cache = fopen("hostname_ip.txt", "r+");
	if(cache == NULL){
		lookup = 1;
	} else { 
		char *line = NULL; 
		size_t len = 0; 
		while(getline(&line, &len, cache) != -1){
			host = strtok(line, ",");
			ip = strtok(NULL, "\n");
			if(strcmp(host, name) == 0 || strcmp(ip, name) == 0){
				lookup = 0;
				break;
			}
		}
	}
	if(lookup){
		char *mapping[100];
		h = gethostbyname(name);
		if(h == NULL){
			(*valid)++;

		} else {
			memcpy(&sin.sin_addr, h->h_addr, h->h_length);
			sprintf(mapping, "%s,%s", name, inet_ntoa(*((struct in_addr*) h->h_addr_list[0])));
			fprintf(cache, "%s\n", mapping);
		}
	} else {
		sin.sin_addr.s_addr = inet_addr(ip);
	}
	fclose(cache);
	pthread_mutex_unlock(&ip_lock);
	return sin;

}

int check_blacklist(char *name, char *internet_protocol){
	pthread_mutex_lock(&black_lock);
	FILE *blist = fopen("blacklist.txt", "r");
	if(blist == NULL){
		return -1;
	} else { 
		char *line = NULL;
		size_t len = 0; 
		while(getline(&line, &len, blist) != -1){
			if(line[strlen(line) - 1] == '\n'){
				line[strlen(line) - 1] = '\0';
			}
			if(strcmp(line, name) == 0 || strcmp(internet_protocol, line) == 0){
				return -1;
			}
		}
	}
	fclose(blist);
	pthread_mutex_unlock(&black_lock);
	return 0;
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

int check_cache_avail(char *hash){
	pthread_mutex_lock(&pg_lock);
	FILE *pg = fopen("page_avail.txt", "ab+");
	int found = 0;
	if(pg == NULL){
		found = 0;
	} else {
		char *line = NULL;
		size_t len = 0; 
		while(getline(&line, &len, pg) != -1){
			if(line[strlen(line)-1] == '\n'){
				line[strlen(line)-1] = '\0';
			}
			if(strcmp(hash, line) == 0){
				found = 1;
				break;
			}
		}
	}
	if(!found){
		fprintf(pg, "%s\n", hash);
	}
	fclose(pg);
	pthread_mutex_unlock(&pg_lock);
	return found;
}

int send_cached(char *hash, int connfd){
	printf("SENDING CACHED\n");
	char *path[39];
	sprintf(path, "cache/%s", hash);
	FILE *c = fopen(path, "rb");
	if(c == NULL){
		return -1;
	} else {
		char *buf[MAXLINE];
		int r; 
		while((r = fread(buf, 1, MAXLINE, c)) > 0){
			send(connfd, buf, r, MSG_NOSIGNAL);
			bzero(buf, MAXLINE);
		}
	}
	fclose(c);
	return 1;
}

int send_and_write_cache(char *hash, int connfd, int sockfd, char *header){
	printf("WRITING AND SENDING NEW CACHE\n");
	char *path[39];
	sprintf(path, "cache/%s", hash);
	printf("header = %s\n", header);
	write(sockfd, header, strlen(header));
	FILE *c = fopen(path, "ab+");
	if(c == NULL){
		return -1;
	} else {
		char *buf[MAXLINE];
		int r; 
		while((r = read(sockfd, buf, MAXLINE)) > 0){
			send(connfd, buf, r, MSG_NOSIGNAL);
			fwrite(buf, 1, r, c);
			bzero(buf, MAXLINE);
		}
	}
	fclose(c);
	return 1;

}

void proxy(int connfd){
	size_t n; 
	char buf[MAXLINE];
	char cpy[MAXLINE];
	n = read(connfd, buf, MAXLINE);
	strcpy(cpy, buf);
	printf("%s\n", cpy);
	char *full[255];
	char *input = strtok(buf, " ");
	char *name= strtok(NULL, " ");
	strcpy(full, name);

	char internal_error[MAXLINE];
	strcpy(internal_error, "500 Internal Server Error\r\nContent-Type:text/html\r\nContent-Length:49\r\n\r\n<html><h3>An internal server has occurred :(</h3>");
	char notfound[MAXLINE];
	strcpy(notfound, "404 Not Found\r\nContent-Type:text/html\r\nContent-Length:49\r\n\r\n<html><h3>Request not found</h3>");
	char forbidden[MAXLINE];
	strcpy(notfound, "403 Forbidden\r\nContent-Type:text/html\r\nContent-Length:49\r\n\r\n<html><h3>Request not allowed.</h3>");

	if(name != NULL && strcmp(input, "GET") == 0){
		char *type = strtok(NULL, "\n");
		char *rest = strtok(NULL, "");
		char port[4];
		int s_port;
		char *path;
		if(name[4] == 's'){
			memmove(name, name+8, strlen(name));
		} else {
			memmove(name, name+7, strlen(name));
			
		}
		if(strstr(name, ":") != NULL){			
			int go = 0;
			int idx = 0;
			for(int i = 0; name[i] != '\0'; i++){
				if(go == 1 ){
					if(name[i] == '/'){
						go = 0;
					} else {
						port[idx] = name[i];
						idx++;
					}
				}
				if(name[i] == ':'){
					go = 1;
				}

			}
			s_port = atoi(port);
			name = strtok(name, ":");
			char *tmp = strtok(NULL, "/");
			path = strtok(NULL, "");
		} else {
			s_port = 80;
			strtok(name, "/");
			path = strtok(NULL, "");
		}
		if(path == NULL){
			path = "";
		}

		int valid = 0;
		struct sockaddr_in sin = check_hostname_cache(name, s_port, &valid);
		if(valid){
			send(connfd, notfound, strlen(notfound), MSG_NOSIGNAL);
		} else {
			int ret = check_blacklist(name, inet_ntoa(sin.sin_addr)); 
			if(ret != -1){
				char *hash = str2md5(full, strlen(full));
				int cached = check_cache_avail(hash);
				if(cached){
					int succ = send_cached(hash, connfd);
					if(succ == -1){
						send(connfd, internal_error, strlen(internal_error), MSG_NOSIGNAL);
					} 
				} else {

					int sockfd;
					sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
					if(sockfd < 0){
						send(connfd, internal_error, strlen(internal_error), MSG_NOSIGNAL);
					} else {
						if(connect(sockfd, (struct sockaddr*) &sin, sizeof(sin)) != 0){
							send(connfd, internal_error, strlen(internal_error), MSG_NOSIGNAL);
						} else {
							char *header[MAXLINE];
							sprintf(header, "%s /%s %s\n%s\r\n\r\n", input, path, type, rest);
							int succ2 =send_and_write_cache(hash, connfd, sockfd, header);
							if(succ2 == -1){
								send(connfd, internal_error, strlen(internal_error), MSG_NOSIGNAL);
							}
						}
					}
				}
			} else {
				send(connfd, forbidden, strlen(forbidden), MSG_NOSIGNAL);
			}
		}

	} 
	return NULL;
}

int open_listenfd(int port){
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
