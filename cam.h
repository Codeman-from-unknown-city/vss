#pragma once

#include <stddef.h>
#include <linux/videodev2.h>

struct camera;

struct camera* camera(char* path);

void cam_init(struct camera* cam, int pixfmt, int imgh, int imgw, size_t nbufs);

void cam_turn_on(struct camera* cam);

void cam_turn_off(struct camera* cam);

void** cam_grab_frame(struct camera* cam, size_t* size);

