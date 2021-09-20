#pragma once

void setup_camera();

enum CAM_STATE {
	ON,
	OFF
};

void turn_camera(enum CAM_STATE state);

void grab_img_from_camera(void (*process_img)(void* base, size_t size));

