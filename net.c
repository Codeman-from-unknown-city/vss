#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>

#include "utils.h"

#define SAPC(ptr) (struct sockaddr*) ptr
#define NOBACKLOG 0

struct server {
	struct sockaddr_in rem_addr;
	socklen_t addrlen;
	int listenfd;
	int connfd;
	int peerfd;
};

static inline int xsocket(int domain, int type, int protocol)
{
	int sockfd = socket(domain, type, protocol);
	if (sockfd == -1)
		die("Can't create socket");
	return sockfd;
}

static inline void xbind(int sockfd, const struct sockaddr_in* addr,
			socklen_t addrlen)
{
	if (bind(sockfd, SAPC(addr), addrlen) == -1)
		die("Can't bind a name to a socket");	
}

static inline void init_loc_addr(struct sockaddr_in* addr, short port)
{
	memclr(addr, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = htonl(INADDR_ANY);
}

static void setup_tcp(struct server* serv, short port)
{
	serv->listenfd = xsocket(AF_INET, SOCK_STREAM, 0);
	int opts = SO_REUSEADDR | SO_REUSEPORT;
	const int on = 1;
	if (setsockopt(serv->listenfd, SOL_SOCKET, opts, &on, sizeof(on)) == -1)
		die("Can't set socket options");
	struct sockaddr_in loc_addr;
	init_loc_addr(&loc_addr, port);
	xbind(serv->listenfd, &loc_addr, sizeof(loc_addr));
	if (listen(serv->listenfd, NOBACKLOG) == -1)
		die("Can't listen for connections on a socket");
}

static void setup_udp(struct server* serv, short port)
{
	serv->peerfd = xsocket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in loc_addr;
	init_loc_addr(&loc_addr, port);
	xbind(serv->peerfd, &loc_addr, sizeof(loc_addr));
}

struct server* server_setup(short port)
{
	struct server* server = malloc(sizeof(*server));
	if (server == NULL)
		die("Out of memory");
	setup_tcp(server, port);
	setup_udp(server, port);
	return server;
}

void server_wait_connection(struct server* s)
{
	s->addrlen = sizeof(s->rem_addr);
	s->connfd = accept(s->listenfd, SAPC(&s->rem_addr), &s->addrlen);
	if (s->connfd == -1)
		die("Can't accept a connection on a socket");
	char tmp;
	recvfrom(s->peerfd, &tmp, 1, 0, SAPC(&s->rem_addr), &s->addrlen);
}

bool server_client_connected(struct server* s)
{
	char test;
	bool conn_reset = send(s->connfd, &test, 1, MSG_NOSIGNAL) == -1;
	if (conn_reset)
		close(s->connfd);
	return !conn_reset;
}

#define MAX_PCKT_SIZE 65507
#define PCKT_SIZE_FIELD_TYPE uint16_t
#define MAX_DATA_SIZE MAX_PCKT_SIZE - sizeof(PCKT_SIZE_FIELD_TYPE)

struct __attribute__((__packed__)) pckt {
	PCKT_SIZE_FIELD_TYPE size;
	void* data[MAX_DATA_SIZE];
};

static void send_msg(int peerfd, struct sockaddr* rem_addr, socklen_t addrlen,
		     void* data, size_t size)
{
	if (size > MAX_DATA_SIZE) {
		// TODO: make log function
		char* fmt = "send_msg: msg too big (%u)";
		syslog(LOG_WARNING, fmt, size);
		printf(fmt, size);
		return;
	}
	static struct pckt pckt;
	pckt.size = size;
	memcpy(pckt.data, data, size);
	ssize_t ns = 0;
	ssize_t prev  = 0;
	size += sizeof(PCKT_SIZE_FIELD_TYPE);
	while (ns < size) {
		ns += sendto(peerfd, &pckt + ns, size - ns, MSG_NOSIGNAL,
				rem_addr, addrlen);
		if (ns < prev)
			return;
		prev = ns;
	}
}

void server_send_msg(struct server* s, void* data, size_t size)
{
	send_msg(s->peerfd, SAPC(&s->rem_addr), s->addrlen, data, size);
}

