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

int camfd;
void** mmaped_imgs;
int nimgs = NBUFS;

static void open_cam()
{
	struct stat st;
	EXIT_WITH_MSG_IF_SYSCALL_FAILS(stat(CAMNAME, &st), "Cannot identify %s", CAMNAME);
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
		die("VIDIOC_QUERYCAP");
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
	EXIT_IF_SYSCALL_FAILS(ioctl(camfd, VIDIOC_G_FMT, &fmt));
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	EXIT_IF_SYSCALL_FAILS(ioctl(camfd, VIDIOC_S_FMT, &fmt));
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
		EXIT_IF_SYSCALL_FAILS(ioctl(camfd, VIDIOC_QUERYBUF, &buf));
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

static void enqueue_bufs()
{
	struct v4l2_buffer buf;
	for (int i = 0; i < nimgs; i++) {
		memclr(&buf, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		EXIT_IF_SYSCALL_FAILS(ioctl(camfd, VIDIOC_QBUF, &buf));
	}
}

void turn_camera(enum CAM_STATE state)
{
	int action = VIDIOC_STREAMOFF;
	char* errmsg = "VIDIOC_STREAMOFF";
	if (state == ON) {
		enqueue_bufs();
		action = VIDIOC_STREAMON;
		errmsg = "VIDIOC_STREAMON";
	}
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(camfd, action, &type) == -1)
		die(errmsg);
}

static void wait_ready_state()
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(camfd, &fds);
	struct timeval tv;
	for (;;) {
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		int retval = select(camfd + 1, &fds, NULL, NULL, NULL);
		if (retval == -1) {
			if (EINTR == errno) 
				continue;
			die("select");
		}
		if (!retval)
			die("select timeout");
		break;
	}
}

void grab_img_from_camera(void (*process_img)(void* base, size_t size))
{
	wait_ready_state();
	struct v4l2_buffer buf;
	memclr(&buf, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	for (;;) {
		if (-1 == ioctl(camfd, VIDIOC_DQBUF, &buf)) {
			if (EAGAIN == errno)
				continue;
			die("VIDIOC_DQBUF");
		}
		break;
	}
	process_img(mmaped_imgs[buf.index], buf.bytesused);
	EXIT_IF_SYSCALL_FAILS(ioctl(camfd, VIDIOC_QBUF, &buf));
}

