#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PCKT_SIZE 65507

struct server;

struct server* server(char* hostname, uint16_t port);

void server_run(struct server*, 
		void (*conn_handler)(struct server*, int peerfd, 
				     struct sockaddr* rem_addr, 
				     socklen_t addrlen, void* args),
		void* args);

bool server_client_connected(struct server*);

void xsendto(int sockfd, const void* buf, size_t len,
	     const struct sockaddr_in* destaddr, socklen_t addrlen);

