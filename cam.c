#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/videodev2.h>

#include "cam.h"
#include "utils.h"

int cam_open(char* path)
{
	struct stat sb;
	if (stat(path, &sb))
		die("Can't read %s status", path);
	if (!S_ISCHR(sb.st_mode))
		die("%s is no device", path);
	int camfd = open(path, O_RDWR | O_NONBLOCK);
	if (camfd == -1)
		die("Can't open %s", path);
	struct v4l2_capability cap;
	if (ioctl(camfd, VIDIOC_QUERYCAP, &cap) == -1) {
		if (errno == EINVAL)
			die("%s is no V4L2 device", path);
		die("Can't query %s capabilities");
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		die("%s is no video capture device", path);
	if (!(cap.capabilities & V4L2_CAP_STREAMING))
		die("%s does not support streaming i/o", path);
	return camfd;
}

void cam_setfmt(int camfd, int imgh, int imgw, int pixfmt)
{
	struct v4l2_format fmt;
	memclr(&fmt, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(camfd, VIDIOC_G_FMT, &fmt))
		die("Can't get camera pixel format");
	fmt.fmt.pix.pixelformat = pixfmt;
	if (imgh && imgw) {
		fmt.fmt.pix.height = imgh;
		fmt.fmt.pix.width = imgw;
	}
	if (ioctl(camfd, VIDIOC_S_FMT, &fmt))
		die("Can't set camera pixel format");
	if (fmt.fmt.pix.pixelformat != pixfmt)
		die("Camera does not support this pixel format");
	if (imgh && imgw && 
	    fmt.fmt.pix.height != imgh && fmt.fmt.pix.width != imgw) {
		die("Camera does't support %ix%i resolution. "
		    "The driver set %ix%i",
		    imgh, imgw, fmt.fmt.pix.height, fmt.fmt.pix.width);
	}
}

static void req_bufs(int camfd, size_t* nimgs)
{
        struct v4l2_requestbuffers req;
	memclr(&req, sizeof(req));
        req.count = *nimgs;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(camfd, VIDIOC_REQBUFS, &req) == -1) {
		if (errno == EINVAL)
			die("Camera does not support memory mappimg");
		die("Can't reques buffers from camera");
	}
	if (req.count < *nimgs)
		die("Insufficient buffer memory on camera");
	*nimgs = req.count;
}

void** cam_mmap_imgs(int camfd, size_t* nimgs)
{
	req_bufs(camfd, nimgs);
	void** mmaped_imgs = calloc(*nimgs, sizeof(*mmaped_imgs));
        if (mmaped_imgs == NULL)
		die("Out of memory");
	struct v4l2_buffer buf;
        for (int i = 0; i < *nimgs; ++i) {
		memclr(&buf, sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
		if (ioctl(camfd, VIDIOC_QUERYBUF, &buf))
				die("Can't query buffer from driver");
                mmaped_imgs[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, 
				      MAP_SHARED, camfd, buf.m.offset);
		if (mmaped_imgs[i] == MAP_FAILED)
			die("Memory mapping failed");
        }
	return mmaped_imgs;
}

struct v4l2_buffer clipboard = {
	.type = V4L2_BUF_TYPE_VIDEO_CAPTURE, 
	.memory = V4L2_MEMORY_MMAP
};

void cam_enqueue_img(int camfd, size_t index)
{
	clipboard.index = index;
	if (ioctl(camfd, VIDIOC_QBUF, &clipboard) == -1)
		die("Can't enqueue buffer");
}

static void wait_ready_state(int camfd)
{
	fd_set fds;
	FD_ZERO(&fds); FD_SET(camfd, &fds);
	struct timeval tv;
	for (;;) {
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		int retval = select(camfd + 1, &fds, NULL, NULL, NULL);
		if (retval == -1) {
			if (EINTR == errno) 
				continue;
			die("Can't try to wait driver");
		}
		if (!retval)
			die("Driver timeout");
		break;
	}
}

void cam_dequeue_img(int camfd, size_t* index, size_t* size)
{
	wait_ready_state(camfd);
	for (;;) {
		if (ioctl(camfd, VIDIOC_DQBUF, &clipboard)) {
			if (EAGAIN == errno)
				continue;
			die("Can't dequeue buffer");
		}
		break;
	}
	*index = clipboard.index;
	*size = clipboard.bytesused;
}

void cam_turn_on(int camfd, size_t nimgs)
{
	for (size_t i = 0; i < nimgs; i++)
		cam_enqueue_img(camfd, i);
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(camfd, VIDIOC_STREAMON, &type))
			die("Can't start video capture");
}

void cam_turn_off(int camfd)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(camfd, VIDIOC_STREAMOFF, &type))
			die("Can't stop video capture");
}

