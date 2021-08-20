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

#include "utils.h"

#define SAPC(ptr) (struct sockaddr*) ptr
#define PORT 8080
#define NOBACKLOG 0

int listenfd;
int connfd;
int peerfd;

struct sockaddr_in loc_addr;
struct sockaddr_in rem_addr;
socklen_t addrlen = sizeof(struct sockaddr_in);

static void init_loc_addr()
{
	memclr(&loc_addr, addrlen);
	loc_addr.sin_family = AF_INET;
	loc_addr.sin_port = htons(PORT);
	loc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
}

static void setup_tcp()
{
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		die("socket");
	int opts = SO_REUSEADDR | SO_REUSEPORT;
	const int on = 1;
	if (setsockopt(listenfd, SOL_SOCKET, opts, &on, sizeof(on)) == -1)
		die("setsockopt");
	init_loc_addr();
	if (bind(listenfd, SAPC(&loc_addr), addrlen) == -1)
		die("bind");
	if (listen(listenfd, NOBACKLOG) == -1)
		die("listen");
}

static void setup_udp()
{
	if ((peerfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		die("socket");
	init_loc_addr();
	if (bind(peerfd, SAPC(&loc_addr), addrlen) == -1)
		die("bind");
}

void setup_server()
{
	setup_tcp();
	setup_udp();
}

void wait_connection()
{
	addrlen = sizeof(rem_addr);
	if ((connfd = accept(listenfd, SAPC(&rem_addr), &addrlen)) == -1)
		die("accept");
	char tmp;
	recvfrom(peerfd, &tmp, 1, 0, SAPC(&rem_addr), &addrlen);
}

bool client_connected()
{
	if (connfd == -1)
		return false;
	char test;
	bool conn_reset = send(connfd, &test, 1, MSG_NOSIGNAL) == -1;
	if (conn_reset)
		close(connfd);
	return !conn_reset;
}

#define PCKT_SIZE 65472

struct pckt {
	size_t size;
	void* data[PCKT_SIZE - sizeof(size_t)];
};

void send_msg(void* data, size_t size)
{
	static struct pckt pckt;
	pckt.size = size;
	memcpy(pckt.data, data, size);
	ssize_t ns = 0;
	ssize_t prev  = 0;
	while (ns < size) {
		ns += sendto(peerfd, &pckt + ns, size - ns, 0, 
				SAPC(&rem_addr), addrlen);
		if (ns < prev) {
			fprintf(stderr, "Writing error: %s\n", strerror(errno));
			close(connfd);
			connfd = -1;
		}
		prev = ns;
	}
}

