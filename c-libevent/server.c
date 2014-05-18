#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define EXT_PORT 8443
#define SSH_PORT 22
#define SSL_PORT 9443
#define MAX_RECV_BUF 2<<20

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

static struct timeval TIMEOUT = { 60 * 15,  0 };
static struct timeval SSH_TIMEOUT = { 3,  0 }; 

struct otherside {
	struct bufferevent* bev;
	struct otherside* pair;
	uint8_t other_timedout;
};

static void set_tcp_no_delay(evutil_socket_t fd)
{
	int one = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
		&one, sizeof one);
}


static void errorcb(struct bufferevent *bev, short error, void *ctx);
static void initial_read(struct bufferevent *bev, void *ctx);

static void do_pipe(struct bufferevent *bev, void *ctx) {
	struct otherside* con = ctx;
	if (con->bev) {
		con->pair->other_timedout = 0;
		bufferevent_read_buffer(bev, bufferevent_get_output(con->bev));
	}
	else {
		evbuffer_drain(bufferevent_get_input(bev), SIZE_MAX);
	}
}

static struct otherside** create_contexts(struct bufferevent *forward, struct bufferevent *backward) {
	struct otherside **result = calloc(2, sizeof(struct otherside*));
	result[0] = malloc(sizeof(struct otherside));
	result[1] = malloc(sizeof(struct otherside));
	result[0]->bev = backward;
	result[0]->pair = result[1];
	result[0]->other_timedout = 0;
	result[1]->bev = forward;
	result[1]->pair = result[0];
	result[1]->other_timedout = 0;
	return result;
}

static void event_back_connection(struct bufferevent *bev, short events, void *ctx)
{
	struct bufferevent* other_side = ctx;
    if (events & BEV_EVENT_CONNECTED) {
		struct otherside **ctxs = create_contexts(other_side, bev);
		evutil_socket_t fd = bufferevent_getfd(bev);
		set_tcp_no_delay(fd);


    	bufferevent_enable(other_side, EV_READ|EV_WRITE);
		/* pipe already available data to backend */
		bufferevent_read_buffer(other_side, bufferevent_get_output(bev));
        bufferevent_setcb(other_side, do_pipe, NULL, errorcb, ctxs[0]);

        bufferevent_setcb(bev, do_pipe, NULL, errorcb, ctxs[1]);
        bufferevent_setwatermark(bev, EV_READ, 0, MAX_RECV_BUF);
        bufferevent_enable(bev, EV_READ|EV_WRITE);

		bufferevent_set_timeouts(bev, &TIMEOUT, NULL);
		bufferevent_set_timeouts(other_side, &TIMEOUT, NULL);
		free(ctxs);
    } else if (events & BEV_EVENT_ERROR) {
		bufferevent_free(bev);
		if (other_side) {
			bufferevent_free(other_side);
		}
    }
}
static void create_pipe(struct event_base *base, struct bufferevent *other_side, uint32_t port) {
    struct bufferevent *bev;
    struct sockaddr_in sin;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
    sin.sin_port = htons(port); 

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, NULL, NULL, event_back_connection, other_side);

    if (bufferevent_socket_connect(bev, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        /* Error starting connection */
        bufferevent_free(bev);
		if (other_side) {
			bufferevent_free(other_side);
		}
    }
}



static void initial_errorcb(struct bufferevent *bev, short error, void *ctx) {
    if (error & BEV_EVENT_TIMEOUT) {
		/* nothing received so must be a ssh client */
		printf("Nothing received, timeout, assuming ssh\n");
		initial_read(bev, ctx);
		return;
    }
    bufferevent_setcb(bev, NULL, NULL, NULL, NULL);
    bufferevent_free(bev);
}

static void errorcb(struct bufferevent *bev, short error, void *ctx)
{
	struct otherside* con = ctx;
    if (error & BEV_EVENT_EOF) {
        /* connection has been closed, do any clean up here */
        /* ... */
    } else if (error & BEV_EVENT_ERROR) {
        /* check errno to see what error occurred */
        /* ... */
    } else if (error & BEV_EVENT_TIMEOUT) {
		/* re-enable reading and writing to detect future timeouts */
        bufferevent_enable(bev, EV_READ|EV_WRITE);
		if (con->bev) {
			con->pair->other_timedout = 1;
			if (!con->other_timedout) {
				/* 
				 * the other side didn't time-out yet, let's just flag our timeout
				 */
				return;
			}
		}
    }
    /*bufferevent_setcb(bev, NULL, NULL, NULL, NULL); */
    bufferevent_free(bev);
	if (con->bev) {
		/*
		 let the back connection (whichever direction) finish writing it's buffers.
		 But then timeout, but make sure the ctx is changed to avoid writing stuff to a
		 freed buffer.
		 */
		struct timeval ones = { 1, 0};
		bufferevent_set_timeouts(con->bev, &ones, &ones);
        bufferevent_enable(con->bev, EV_READ|EV_WRITE);
		con->pair->pair = NULL;
		con->pair->bev = NULL;
    	bufferevent_setcb(con->bev, do_pipe, NULL, errorcb, con->pair);
	}
	free(con);
}

static void initial_read(struct bufferevent *bev, void *ctx) {
 	struct event_base *base = ctx;
    struct evbuffer *input = bufferevent_get_input(bev);
	uint32_t port = SSH_PORT;

	/* lets peek at the first byte */
    struct evbuffer_iovec v[1];
	if (evbuffer_peek(input, 1, NULL, v, 1) == 1) {

		uint8_t header = *((uint8_t*)(v[0].iov_base));
		if (header == 22 || header == 128) {
			port = SSL_PORT;
		}
	}
    bufferevent_setwatermark(bev, EV_READ, 0, MAX_RECV_BUF);
    bufferevent_disable(bev, EV_READ|EV_WRITE);
	bufferevent_set_timeouts(bev, NULL, NULL);
    bufferevent_setcb(bev, NULL, NULL, errorcb, NULL);
	create_pipe(base, bev, port);
}

/* a new connection arrives */
static void do_accept(evutil_socket_t listener, short UNUSED(event), void *arg)
{
    struct event_base *base = arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);
    if (fd < 0) {
        perror("accept");
    } else if (fd > FD_SETSIZE) {
        close(fd);
    } else {
        struct bufferevent *bev;
        evutil_make_socket_nonblocking(fd);
        bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, initial_read, NULL, initial_errorcb, base);
        bufferevent_setwatermark(bev, EV_READ, 0, MAX_RECV_BUF);
        bufferevent_enable(bev, EV_READ|EV_WRITE);
		bufferevent_set_timeouts(bev, &SSH_TIMEOUT, NULL);
    }
}

static void run(void)
{
    evutil_socket_t listener;
    struct sockaddr_in sin;
    struct event_base *base;
    struct event *listener_event;

    base = event_base_new();
    if (!base)
        return; 

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(EXT_PORT);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    evutil_make_socket_nonblocking(listener);

#ifndef WIN32
    {
        int one = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    }
#endif

    if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("bind");
        return;
    }

    if (listen(listener, 16)<0) {
        perror("listen");
        return;
    }

    listener_event = event_new(base, listener, EV_READ|EV_PERSIST, do_accept, (void*)base);

    event_add(listener_event, NULL);

    event_base_dispatch(base);
}

int
main(int UNUSED(c), char **v)
{
	(void)v;
    setvbuf(stdout, NULL, _IONBF, 0);

    run();
    return 0;
}
