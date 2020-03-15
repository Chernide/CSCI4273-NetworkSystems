CC = gcc
CFLAGS = -w -g -Wall -pthread
LDFLAGS = -pthread

all: client server

client: client.c 
	$(CC) $(CFLAGS) -o dfc client.c -lcrypto -lssl

server: server.c 
	$(CC) $(CFLAGS) -o dfs server.c -lcrypto -lssl

clean:
	$(RM) dfc
	$(RM) dfs
	$(RM) -rf ./DFS1/*
	$(RM) -rf ./DFS2/*
	$(RM) -rf ./DFS3/*
	$(RM) -rf ./DFS4/*

