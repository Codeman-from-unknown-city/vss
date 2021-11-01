#pragma once

#include <stddef.h>
#include <linux/videodev2.h>

int cam_open(char* path);

void cam_setfmt(int camfd, int imgh, int imgw, int pixfmt);

void** cam_mmap_imgs(int camfd, size_t* nimgs);

void cam_enqueue_img(int camfd, size_t index);

void cam_dequeue_img(int camfd, size_t* index, size_t* size);

void cam_turn_on(int camfd, size_t nimgs);

void cam_turn_off(int camfd);

