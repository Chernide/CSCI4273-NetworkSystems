CC = gcc
CFLAGS = -w -g -Wall -pthread
LDFLAGS = -pthread
all: webproxy.c 
	$(CC) $(CFLAGS) -o webproxy webproxy.c -lcrypto -lssl

clean:
	$(RM) webproxy


