#include <stdio.h>

#include "daemon.h"
#include "cam.h"
#include "net.h"
#include "utils.h"

int main()
{
	if (become_daemon() == -1)
		die("Cannot become daemon");
	setup_camera();
	setup_server();
	while (true) {
		wait_connection();
		turn_on_camera();
		while (client_connected())
			grab_img_from_camera(send_msg);
		turn_off_camera();
	}
	return 0;
}

