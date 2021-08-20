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

#define CAMNAME "/dev/video0"
#define NBUFS 20
#define VHEIGHT 720
#define VWIDTH 1280

int camfd;
void** mmaped_imgs;
int nimgs = NBUFS;

static void open_cam()
{
	struct stat st;
	if (stat(CAMNAME, &st) == -1)
		die("Cannot identify %s", CAMNAME);
	if (!S_ISCHR(st.st_mode)) 
		die("%s is no device", CAMNAME);
	if ((camfd = open(CAMNAME, O_RDWR | O_NONBLOCK)) == -1)
		die("Cannot open %s", CAMNAME);
}

static void test_cap()
{
	struct v4l2_capability cap;
	if (ioctl(camfd, VIDIOC_QUERYCAP, &cap) == -1) {
		if (errno == EINVAL)
			die("%s is no V4L2 device", CAMNAME);
		die("ioctl");
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		die("%s is no video capture device", CAMNAME);
	if (!(cap.capabilities & V4L2_CAP_STREAMING))
		die("%s does not support streaming i/o", CAMNAME);
}

static void setfmt()
{
	struct v4l2_format fmt;
	memclr(&fmt, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(camfd, VIDIOC_G_FMT, &fmt) == -1)
		die("VIDIOC_G_FMT");
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix.width = VWIDTH;
	fmt.fmt.pix.height = VHEIGHT;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	if (ioctl(camfd, VIDIOC_S_FMT, &fmt) == -1)
		die("VIDIOC_S_FMT");
	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG)
		die("Camera does not support mjpeg pixel format");
}

static void init_mmap()
{
        struct v4l2_requestbuffers req;
	memclr(&req, sizeof(req));
        req.count = NBUFS;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(camfd, VIDIOC_REQBUFS, &req) == -1) {
		if (errno == EINVAL)
			die("Camera does not support memory mappimg");
		die("VIDIOC_REQBUFS");
	}
	if (req.count < NBUFS)
		die("Insufficient buffer memory on camera");
}

static void mmap_imgs()
{
        if ((mmaped_imgs = calloc(nimgs, sizeof(*mmaped_imgs))) == NULL)
		die("calloc");
	struct v4l2_buffer buf;
        for (int i = 0; i < nimgs; ++i) {
		memclr(&buf, sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
		if (ioctl(camfd, VIDIOC_QUERYBUF, &buf) == -1)
			die("VIDIOC_QUERYBUF");
                mmaped_imgs[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, 
						MAP_SHARED, camfd, buf.m.offset);
		if (mmaped_imgs[i] == MAP_FAILED)
			die("mmap");
        }
}

static void init_cam()
{
	setfmt();
	init_mmap();
	mmap_imgs();
}

void setup_camera()
{
	open_cam();
	test_cap();
	init_cam();
}

void turn_on_camera()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	struct v4l2_buffer buf;
	for (int i = 0; i < nimgs; i++) {
		memclr(&buf, sizeof(buf));
		buf.type = type;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (ioctl(camfd, VIDIOC_QBUF, &buf) == -1)
			die("VIDIOC_QBUF");
	}
	if (ioctl(camfd, VIDIOC_STREAMON, &type) == -1)
		die("VIDIOC_STREAMON");
}

void turn_off_camera()
{
    	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(camfd, VIDIOC_STREAMOFF, &type) == -1)
		die("VIDIOC_STREAMOFF");
}

static void wait_ready_state()
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(camfd, &fds);
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	int retval = select(camfd + 1, &fds, NULL, NULL, &tv);
	if (retval == -1) {
		if (EINTR == errno) {
			puts("Waiting...");
			wait_ready_state();
		}
		else
			die("select");
	}
	if (!retval)
		die("select timeout");
}

void grab_img_from_camera(void (*process_img)(void* base, size_t size))
{
	struct v4l2_buffer buf;
	wait_ready_state();
	memclr(&buf, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (-1 == ioctl(camfd, VIDIOC_DQBUF, &buf)) {
		if (EAGAIN == errno) {
			grab_img_from_camera(process_img);
			return;
		}
		die("Can not grab img");
	}
	process_img(mmaped_imgs[buf.index], buf.bytesused);
	if (-1 == ioctl(camfd, VIDIOC_QBUF, &buf))
		die("VIDIOC_QBUF");
}

