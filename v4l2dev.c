#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include "v4l2dev.h"
#ifndef	SOFTWARE_YUV422_TO_YUV420
#include "mxc_ipu.h"
#endif
#include "mxc_vpu.h"
#ifdef USE_FMT_MJPG
#include "mjpeg.h"
#endif

/**
 * @brief Handle object of V4L2 devices
 * @details You should <b>NOT</b> access this data structure directly.
 */
struct v4l2dev
{
	/** @brief File descriptor of the v4l2 device */
	int fd;

	/** @brief Size of single mmap_buffer */
	int buffer_size;

	/** @brief Number of actually valid buffers */
	int n_valid_buffers;

	/** @brief Memory mapped RAW buffer */
	unsigned char**	mmap_buffers;

	/** @brief Output buffer for user reading */
	unsigned char*	buffer;

	/** @brief Current width */
	int width;

	/** @brief Current height */
	int height;

#ifndef USE_YUV422_OUTPUT
#ifndef	SOFTWARE_YUV422_TO_YUV420
	/** @brief IPU handle from libipu */
	ipu_lib_handle_t* ipu_handle;
#endif
#endif
	DecodingInstance decoding;

};

#ifndef USE_FMT_MJPG
#ifdef SOFTWARE_YUV422_TO_YUV420
static void convert_yuv422_to_yuv420(unsigned char *InBuff, unsigned char *OutBuff, int width, int height)
{
	int i, j;
	unsigned char* in;
	unsigned char* in2;
	unsigned char* out;
	unsigned char* out2;

	/* Write Y plane */
	for(i = 0;i < width * height; ++i)
		OutBuff[i] = InBuff[i * 2];


	/* Write UV plane */
	for(j = 0;j < height / 2; ++j)
	{
		in = &(InBuff[j * 2 * width * 2]);
		in2 = &(InBuff[(j * 2 + 1)* width * 2]);
		out = &(OutBuff[width * height + j * width / 2]);
		out2 = &(OutBuff[width * height * 5 / 4 + j * width / 2]);
		for(i = 0;i < width / 2; ++i)
		{
			out[i] = (in[i * 4 + 1] + in2[i * 4 + 1]) / 2;
			out2[i] = (in[i * 4 + 3] + in2[i * 4 + 3]) / 2;
		}

	}

}
#endif
#endif

static int is_valid_v4l2dev(v4l2dev device)
{
	if(!device) return 0;
	if(device->fd<0) return 0;
	if(device->n_valid_buffers<1) return 0;
	if(!device->mmap_buffers) return 0;
	return 1;
}

static int xioctl (int fd, int request, void* arg)
{
	int r;

	do
	{
		usleep(1000);
		r = ioctl(fd, request, arg);
	}
	while(-1 == r && EINTR == errno);

	return r;
}


v4l2dev v4l2dev_open(const char* device_node)
{
	v4l2dev device = NULL;
	int fd = 0;

	if(!device_node) return NULL;

	fd = open(device_node, O_RDWR|O_NONBLOCK, 0);

	if(fd == -1) return NULL;

	device = calloc(1, sizeof(struct v4l2dev));
	device->fd = fd;

	return device;
}

void v4l2dev_init(v4l2dev device, const int width, const int height, const int n_buffers)
{
	int input_index = 0;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
	enum v4l2_buf_type type;
	int result;

	int i;

	if(!device) return;

	/* Select input 0 */

	result = xioctl(device->fd, VIDIOC_S_INPUT, &input_index);


	/* Set pixel format */

	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width	= width;
	fmt.fmt.pix.height	= height;
#ifdef USE_FMT_MJPG
	fmt.fmt.pix.pixelformat	= V4L2_PIX_FMT_MJPEG;
#else
	fmt.fmt.pix.pixelformat	= V4L2_PIX_FMT_YUYV;
#endif

	if(-1 == xioctl(device->fd, VIDIOC_S_FMT, &fmt))
	{
		fprintf(stderr, "Unsupported pixel format!\n");
	}


	/* Get real pixel format */

	xioctl(device->fd, VIDIOC_G_FMT, &fmt);
	device->width	= fmt.fmt.pix.width;
	device->height	= fmt.fmt.pix.height;


	memset(&req, 0, sizeof(struct v4l2_requestbuffers));
	req.count               = n_buffers;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;
	xioctl(device->fd, VIDIOC_REQBUFS, &req);

	device->mmap_buffers = calloc(n_buffers, sizeof(unsigned char*));

	for(i = 0;i < n_buffers;i++)
	{
		memset(&buf, 0, sizeof(struct v4l2_buffer));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;

		result = xioctl(device->fd, VIDIOC_QUERYBUF, &buf);
		device->buffer_size = buf.length;
		device->mmap_buffers[i] = mmap (NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED,device->fd, buf.m.offset);

		if(MAP_FAILED == device->mmap_buffers[i]) break;
	}

	device->n_valid_buffers = i;

#ifdef USE_FMT_MJPG
	/* The output pixel format is rgb24 */

	device->buffer = calloc(1, device->width * device->height * 3);
#else
	/* The output pixel format is yuv420p */

	device->buffer = calloc(1, device->width * device->height * 3 / 2);
#endif

	for(i = 0;i < device->n_valid_buffers;i++)
	{
		memset(&buf, 0, sizeof(struct v4l2_buffer));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;

		result = xioctl(device->fd, VIDIOC_QBUF, &buf);
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	xioctl(device->fd, VIDIOC_STREAMON, &type);

#ifndef SOFTWARE_YUV422_TO_YUV420
	/* Setup hardware accelerated YUYV to I420 conversion and IPU display */

	device->ipu_handle = ipu_init(device->width, device->height, IPU_PIX_FMT_YUYV, device->width, device->height, IPU_PIX_FMT_YUV420P, 0);
#endif

//	device->decoding = vpu_create_decoding_instance(device->buffer, MEMORYBLOCK, STD_MJPG);

}

void v4l2dev_close(v4l2dev* device)
{
	enum v4l2_buf_type type;
	int i;
	v4l2dev ptr = *device;

	if(!device) return;
	if(!ptr) return;


	/* Stop capturing */

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	xioctl(ptr->fd, VIDIOC_STREAMOFF, &type);


	/* Unmap buffers */

	if(ptr->n_valid_buffers)
	{
		for(i = 0;i< ptr->n_valid_buffers;++i)
		{
			munmap(ptr->mmap_buffers[i], ptr->buffer_size);
		}
	}

#ifndef SOFTWARE_YUV422_TO_YUV420
	ipu_uninit(&(ptr->ipu_handle));
#endif

	/* Free memory */

	if(ptr->mmap_buffers) free(ptr->mmap_buffers);
	if(ptr->buffer) free(ptr->buffer);
	if(ptr->fd >=0) close(ptr->fd);
	free(ptr);
	*device = NULL;
}

size_t v4l2dev_get_buffersize(v4l2dev device)
{
	if(!is_valid_v4l2dev(device)) return 0;
	return device->buffer_size*3/4;
}

unsigned char* v4l2dev_read(v4l2dev device)
{
	fd_set fds;
	struct timeval tv;
	struct v4l2_buffer buf;
	int result;

	if(!is_valid_v4l2dev(device)) return NULL;

	FD_ZERO(&fds);
	FD_SET(device->fd, &fds);

	while(1)
	{
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		result = select(device->fd + 1, &fds, NULL, NULL, &tv);

		if(result == -1)
		{
			if (EINTR == errno)
				continue;
		}

		if(!result)
		{
			fprintf(stderr, "select timeout\n");
			return NULL;
		}

		memset(&buf, 0, sizeof(struct v4l2_buffer));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;

		result = xioctl(device->fd, VIDIOC_DQBUF, &buf);

		if(result == -1)
		{
			if(errno == EAGAIN)
			{
				fprintf(stderr, "Waiting...\n");
				continue;
			}else return NULL;
		}

#ifdef USE_FMT_MJPG
		jpeg_to_raw(device->mmap_buffers[buf.index], buf.bytesused, device->buffer);
#else
#ifdef	SOFTWARE_YUV422_TO_YUV420
		convert_yuv422_to_yuv420(device->mmap_buffers[buf.index], device->buffer, device->width, device->height);
#else
		ipu_buffer_update(device->ipu_handle, device->mmap_buffers[buf.index], device->buffer);
#endif
#endif
		/* Unlock */
		result = xioctl(device->fd, VIDIOC_QBUF, &buf);

		return device->buffer;
	}
	return NULL;
}

