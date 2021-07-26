#include <stdio.h>

#include "cam.h"
#include "net.h"

int main()
{
	setup_camera();
	setup_server();
	struct img img;
	while (true) {
		puts("Waiting for connetion...");
		wait_connection();
		puts("Got connection!");
		turn_on_camera();
		while (client_connected()) {
			grab_img_from_camera(&img);
			send_msg(img.base, img.size);
		}
		turn_off_camera();
		puts("Connection handled\n");
	}
	return 0;
}

