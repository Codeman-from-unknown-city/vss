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

int ip_named_socket(int type, char* hostname, unsigned short port)
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

int net_wait_connection(int listenfd, int peerfd, struct sockaddr_in* rem_addr,
			 socklen_t* addrlen)
{
	*addrlen = sizeof(*rem_addr);
	int connfd = accept(listenfd, SAPC(rem_addr), addrlen);
	if (connfd == -1)
		die("Can't accept a connection on a socket");
	char tmp;
	recvfrom(peerfd, &tmp, 1, 0, SAPC(rem_addr), addrlen);
	return connfd;
}

bool net_client_connected(int connfd)
{
	char test;
	return !(send(connfd, &test, 1, MSG_NOSIGNAL) == -1);
}

void xsendto(int sockfd, const void* buf, size_t len, 
	     const struct sockaddr_in* destaddr, socklen_t addrlen)
{
	ssize_t ns_total = 0;
	while (ns_total < len) {
		ssize_t ns = sendto(sockfd, buf + ns_total, len - ns_total, 
				    MSG_NOSIGNAL, SAPC(destaddr), addrlen);
		if (ns == -1) // TODO
			return;
		ns_total += ns;
	}
}

