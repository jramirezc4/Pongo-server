CC = gcc
CFLAGS = -Wall -Wextra

ifeq ($(OS),Windows_NT)
	LIBS = -lws2_32
else
	LIBS =
endif

server: server.c protocol.h
	$(CC) $(CFLAGS) server.c $(LIBS) -o server

clean:
	rm -f server
