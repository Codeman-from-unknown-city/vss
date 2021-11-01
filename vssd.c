#include <stdio.h>
#include <unistd.h>

#include "daemon.h"
#include "cam.h"
#include "net.h"
#include "utils.h"

#define HOSTNAME NULL
#define PORT 8080

int main()
{
#ifdef __DAEMON__
	become_daemon();
#endif
	int camfd = cam_open("/dev/video0");
	cam_setfmt(camfd, 0, 0, V4L2_PIX_FMT_MJPEG);
	size_t nimgs = 8;
	void** imgs = cam_mmap_imgs(camfd, &nimgs);
	int listenfd = ip_named_socket(SOCK_STREAM, HOSTNAME, PORT);
	if (listen(listenfd, 0))
		die("Can't listen connections on a socket");
	int peerfd = ip_named_socket(SOCK_DGRAM, HOSTNAME, PORT);
	for (;;) {
		struct sockaddr_in rem_addr;
		socklen_t addrlen;
		int connfd = net_wait_connection(listenfd, peerfd, 
						 &rem_addr, &addrlen);
		cam_turn_on(camfd, nimgs);
		while (net_client_connected(connfd)) {
			size_t i;
			size_t sz;
			cam_dequeue_img(camfd, &i, &sz);
			xsendto(peerfd, imgs[i], sz, &rem_addr, addrlen);
			cam_enqueue_img(camfd, i);
		}
		close(connfd);
		cam_turn_off(camfd);
	}
}

