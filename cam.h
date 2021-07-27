#pragma once

void setup_camera();

void turn_on_camera();

void turn_off_camera();

void grab_img_from_camera(void (*process_img)(void* base, size_t size));

