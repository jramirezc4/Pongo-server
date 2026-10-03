CC = gcc
CFLAGS = -Wall -Wextra

server: server.c protocol.h
	$(CC) $(CFLAGS) server.c -o server

clean:
	rm -f server
