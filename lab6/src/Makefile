CC = gcc
AR = ar
CFLAGS = -std=gnu11 -O2 -Wall -Wextra -pthread

.PHONY: all clean

all: client server

client: client.o libcommon.a
	$(CC) $(CFLAGS) $^ -o $@

server: server.o libcommon.a
	$(CC) $(CFLAGS) $^ -o $@

client.o: client.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

server.o: server.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

common.o: common.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

libcommon.a: common.o
	$(AR) rcs $@ $^

clean:
	rm -f client server *.o libcommon.a
