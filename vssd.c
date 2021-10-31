#include <stdio.h>

#include "daemon.h"
#include "cam.h"
#include "net.h"
#include "utils.h"

void server_send_msg_adapter(void* base, size_t size, void* serv)
{
	server_send_msg((struct server*) serv, base, size);
}

int main()
{
#ifdef __DAEMON__
	become_daemon();
#endif
	int camfd = cam_open("/dev/video0");
	cam_setfmt(camfd, 0, 0, V4L2_PIX_FMT_MJPEG);
	cam_setup(camfd);
	struct server* serv = server_setup(8080);
	while (true) {
		server_wait_connection(serv);
		cam_turn_on(camfd);
		while (server_client_connected(serv))
			cam_grab_and_process_img(camfd, 
						 server_send_msg_adapter, serv);
		cam_turn_off(camfd);
	}
}
