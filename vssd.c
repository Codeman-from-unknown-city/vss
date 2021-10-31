#include <stdio.h>

#include "daemon.h"
#include "cam.h"
#include "net.h"
#include "utils.h"

int main()
{
#ifdef __DAEMON__
	become_daemon();
#endif
	setup_camera();
	setup_server();
	while (true) {
		wait_connection();
		turn_camera(ON);
		while (client_connected())
			grab_img_from_camera(send_msg);
		turn_camera(OFF);
	}
	return 0;
}

