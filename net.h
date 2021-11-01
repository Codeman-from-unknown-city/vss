#pragma once

#include <stdbool.h>
#include <stddef.h>

#define MAX_PCKT_SIZE 65507

int ip_named_socket(int type, char* hostname, unsigned short port);

int net_wait_connection(int listenfd, int peerfd, struct sockaddr_in* rem_addr,
			 socklen_t* addrlen);

bool net_client_connected(int connfd);

void xsendto(int sockfd, const void* buf, size_t len,
	     const struct sockaddr_in* destaddr, socklen_t addrlen);

