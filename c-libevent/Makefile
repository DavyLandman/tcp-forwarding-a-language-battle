LIBEVENT ?= /usr/local
CFLAGS = -I. -I$(LIBEVENT)/include -Wall -Wpedantic -Wextra -g -O2
LIBS = -L. -L$(LIBEVENT)/lib -levent

server: server.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)


clean:
	rm -f *.o server
