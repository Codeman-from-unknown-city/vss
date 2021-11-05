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

struct camera {
	int fd;
	struct {
		void* base;
		size_t size;
	}* mmaped_bufs;
	size_t alloc;
	struct v4l2_buffer clipboard;
};

struct camera* camera(char* path)
{
	struct stat sb;
	if (stat(path, &sb))
		die("Can't read %s status", path);
	if (!S_ISCHR(sb.st_mode))
		die("%s is no device", path);
	int fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd == -1)
		die("Can't open %s", path);
	struct v4l2_capability cap;
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
		if (errno == EINVAL)
			die("%s is no V4L2 device", path);
		die("Can't query %s capabilities");
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		die("%s is no video capture device", path);
	if (!(cap.capabilities & V4L2_CAP_STREAMING))
		die("%s does not support streaming i/o", path);
	struct camera* cam = malloc(sizeof(*cam));
	cam->fd = fd;
	cam->clipboard.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cam->clipboard.memory = V4L2_MEMORY_MMAP;
	return cam;
}

#define DEFAULT_NBUFS 4
#define MIN_NBUFS 2

static size_t req_bufs(int camfd, size_t nbufs)
{
        struct v4l2_requestbuffers req;
	memclr(&req, sizeof(req));
        req.count = nbufs == 0 ? DEFAULT_NBUFS : nbufs;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(camfd, VIDIOC_REQBUFS, &req) == -1) {
		if (errno == EINVAL)
			die("Camera does not support memory mappimg");
		die("Can't reques buffers from camera");
	}
	if (req.count < MIN_NBUFS)
		die("Insufficient buffer memory on camera");
	return req.count;
}

static void mmap_bufs(struct camera* cam, size_t nbufs)
{
	cam->alloc = req_bufs(cam->fd, nbufs);
	cam->mmaped_bufs = calloc(cam->alloc, sizeof(*cam->mmaped_bufs));
        if (cam->mmaped_bufs == NULL)
		die("Out of memory");
        for (int i = 0; i < cam->alloc; ++i) {
                cam->clipboard.index = i;
		if (ioctl(cam->fd, VIDIOC_QUERYBUF, &cam->clipboard))
				die("Can't query buffer from driver");
		cam->mmaped_bufs[i].size = cam->clipboard.length;
                cam->mmaped_bufs[i].base = mmap(NULL, cam->clipboard.length, 
						PROT_READ | PROT_WRITE, 
						MAP_SHARED, cam->fd, 
						cam->clipboard.m.offset);
		if (cam->mmaped_bufs[i].base == MAP_FAILED)
			die("Memory mapping failed");
        }
}

void cam_init(struct camera* cam, int pixfmt, int imgh, int imgw, size_t nbufs)
{
	struct v4l2_format fmt;
	memclr(&fmt, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(cam->fd, VIDIOC_G_FMT, &fmt))
		die("Can't get camera pixel format");
	fmt.fmt.pix.pixelformat = pixfmt;
	if (imgh && imgw) {
		fmt.fmt.pix.height = imgh;
		fmt.fmt.pix.width = imgw;
	}
	if (ioctl(cam->fd, VIDIOC_S_FMT, &fmt))
		die("Can't set camera pixel format");
	if (fmt.fmt.pix.pixelformat != pixfmt)
		die("Camera does not support this pixel format");
	if (imgh && imgw && 
	    fmt.fmt.pix.height != imgh && fmt.fmt.pix.width != imgw) {
		die("Camera does't support %ix%i resolution. "
		    "The driver set %ix%i",
		    imgh, imgw, fmt.fmt.pix.height, fmt.fmt.pix.width);
	}
	mmap_bufs(cam, nbufs);
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

void cam_turn_on(struct camera* cam)
{
	for (size_t i = 0; i < cam->alloc; i++) {
		cam->clipboard.index = i;
		if (ioctl(cam->fd, VIDIOC_QBUF, &cam->clipboard) == -1)
			die("Can't enqueue buffer");
	}
	if (ioctl(cam->fd, VIDIOC_STREAMON, &cam->clipboard.type))
			die("Can't start video capture");
}

void cam_turn_off(struct camera* cam)
{
	if (ioctl(cam->fd, VIDIOC_STREAMOFF, &cam->clipboard.type))
			die("Can't stop video capture");
}

void** cam_grab_frame(struct camera* cam, size_t* size)
{
	if (cam->clipboard.flags != V4L2_BUF_FLAG_QUEUED)
		ioctl(cam->fd, VIDIOC_QBUF, &cam->clipboard);
	wait_ready_state(cam->fd);
	for (;;) {
		if (ioctl(cam->fd, VIDIOC_DQBUF, &cam->clipboard)) {
			if (EAGAIN == errno)
				continue;
			die("Can't dequeue buffer");
		}
		break;
	}
	*size = cam->clipboard.bytesused;
	return cam->mmaped_bufs[cam->clipboard.index].base;
}

