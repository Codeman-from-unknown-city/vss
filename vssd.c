#include <stdio.h>
#include <unistd.h>

#include "daemon.h"
#include "cam.h"
#include "net.h"
#include "utils.h"

#define HOSTNAME NULL
#define PORT 8080

void handle_conn(struct server* srv, int peerfd, 
		 struct sockaddr* rem_addr, socklen_t addrlen, void* args)
{
	struct camera* cam = args;
	cam_turn_on(cam);
	while (server_client_connected(srv)) {
		size_t sz;
		void* frame = cam_grab_frame(cam, &sz);
		if (sendto(peerfd, frame, sz, MSG_NOSIGNAL, rem_addr, addrlen) == -1)
			die("Sending error");
	}
	cam_turn_off(cam);
}

int main()
{
#ifdef __DAEMON__
	become_daemon();
#endif
	struct camera* cam = camera("/dev/video0");
	cam_init(cam, V4L2_PIX_FMT_MJPEG, 0, 0, 0); // Default values
	struct server* srv = server(HOSTNAME, PORT);
	server_run(srv, handle_conn, cam);
	return 0;
}

