CC=gcc
CFLAGS=-Wall -Werror
PROJECT=

all:  server
	
server: server.c
	$(CC) $(CFLAGS) $^ -lsocket  -lnsl -o $@

clean:
	rm -f *.o $(PROJECT)


