#pragma once

#include <stddef.h>
#include <linux/videodev2.h>

int cam_open(char* path);

void cam_setfmt(int camfd, int imgh, int imgw, int pixfmt);

void cam_setup(int camfd);

void cam_turn_on(int camfd);

void cam_turn_off(int camfd);

void cam_grab_and_process_img(
	int camfd,
	void (*process_img)(void* base, size_t size, void* args),
	void* args
);

