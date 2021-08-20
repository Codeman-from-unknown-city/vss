#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

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
	char test;
	bool conn_reset = send(connfd, &test, 1, 0) == -1;
	if (conn_reset)
		close(connfd);
	return !conn_reset;
}

#define PCKT_SIZE 65472

#define min(a, b) a > b ? b : a

#pragma pack(1)
struct header {
	uint8_t num;
	uint16_t size;
};

#define HSIZE sizeof(struct header)

struct pckt {
	struct header header;
	char data[PCKT_SIZE - HSIZE];
};
#pragma pack(0)

static void buildpckt(struct pckt* pckt, uint8_t num, 
			uint16_t size, char* data)
{
	pckt->header.num = num;
	pckt->header.size = htons(size);
	memcpy(pckt->data, data, size);
}

static void sendall(int sockfd, void* data, size_t size, 
			struct sockaddr* addr, socklen_t addrlen)
{
	ssize_t nsent = 0;
	while (nsent < size)
		nsent += sendto(sockfd, data, size, 0, addr, addrlen);
}

void send_msg(void* data, size_t size)
{
	struct pckt pckt;
	uint8_t npckts = (uint8_t) ceil((double) size / (PCKT_SIZE - HSIZE));
	uint16_t send_data_size;
	while (npckts) {
		send_data_size = min(size, PCKT_SIZE - HSIZE);
		buildpckt(&pckt, npckts, send_data_size, data);
		sendall(peerfd, &pckt, PCKT_SIZE, SAPC(&rem_addr), addrlen);
		size -= send_data_size;
		data += send_data_size;
		npckts--;
	}
}

