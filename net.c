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

struct server {
	int listenfd;
	int connfd;
	int peerfd;
};

static int ip_named_socket(int type, char* hostname, unsigned short port)
{
	int sockfd = socket(AF_INET, type, 0);
	if (sockfd == -1)
		die("Can't create a socket");
	int opts = SO_REUSEADDR | SO_REUSEPORT;
	const int on = 1;
	if (setsockopt(sockfd, SOL_SOCKET, opts, &on, sizeof(on)) == -1)
		die("Can't set socket options");
	struct sockaddr_in loc_addr;
	loc_addr.sin_family = AF_INET;
	loc_addr.sin_port = htons(port);
	if (hostname && !inet_aton(hostname, &loc_addr.sin_addr))
		die("%s is invalid internet address");
	else
		loc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sockfd, SAPC(&loc_addr), sizeof(loc_addr)) == -1)
		die("Can't bind a name to a socket");	
	return sockfd;
}

struct server* server(char* hostname, uint16_t port)
{
	struct server* srv = malloc(sizeof(*srv));
	srv->listenfd = ip_named_socket(SOCK_STREAM, hostname, port);
	if (listen(srv->listenfd, 0))
		die("Can't listen connections on a socket");
	srv->peerfd = ip_named_socket(SOCK_DGRAM, hostname, port);
	return srv;
}

void server_run(struct server* srv, 
		void (*conn_handler)(struct server*, int peerfd, 
				     struct sockaddr* rem_addr, 
				     socklen_t addrlen, void* args), 
		void* args)
{
	struct sockaddr_in rem_addr;
	for (;;) {
		socklen_t addrlen = sizeof(rem_addr);
		srv->connfd = accept(srv->listenfd, SAPC(&rem_addr), &addrlen);
		if (srv->connfd == -1)
			die("Can't accept a connection on a socket");
		char req;
		recvfrom(srv->peerfd, &req, 1, 0, SAPC(&rem_addr), &addrlen);
		conn_handler(srv, srv->peerfd, SAPC(&rem_addr), addrlen, args);
	}
	close(srv->connfd);
}

bool server_client_connected(struct server* srv)
{
	char test;
	return !(send(srv->connfd, &test, 1, MSG_NOSIGNAL) == -1);
}

