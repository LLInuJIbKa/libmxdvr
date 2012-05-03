/**
 * @file v4l2dev.c
 * @author Ruei-Yuan Lu (ryuan_lu@iii.org.tw)
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "v4l2dev.h"

#ifndef	SOFTWARE_YUV422_TO_YUV420
#include "mxc_ipu.h"
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
};

#ifdef SOFTWARE_YUV422_TO_YUV420
static void convert_yuv422_to_yuv420(unsigned char *InBuff, unsigned char *OutBuff, int width, int height)
{
	int i = 0, j = 0, k = 0;
	int UOffset = width * height;
	int VOffset = (width * height) * 5 / 4;
	int line1 = 0, line2 = 0;
	int m = 0, n = 0;
	int y = 0, u = 0, v = 0;
	u = UOffset;
	v = VOffset;
	for (i = 0, j = 1; i < height; i += 2, j += 2)
	{
		/* Input Buffer Pointer Indexes */
		line1 = i * width * 2;
		line2 = j * width * 2;
		/* Output Buffer Pointer Indexes */
		m = width * y;
		y = y + 1;
		n = width * y;
		y = y + 1;
		/* Scan two lines at a time */
		for (k = 0; k < width * 2; k += 4)
		{
			unsigned char Y1, Y2, U, V;
			unsigned char Y3, Y4, U2, V2;
			/* Read Input Buffer */
			Y1 = InBuff[line1++];
			U = InBuff[line1++];
			Y2 = InBuff[line1++];
			V = InBuff[line1++];
			Y3 = InBuff[line2++];
			U2 = InBuff[line2++];
			Y4 = InBuff[line2++];
			V2 = InBuff[line2++];
			/* Write Output Buffer */
			OutBuff[m++] = Y1;
			OutBuff[m++] = Y2;
			OutBuff[n++] = Y3;
			OutBuff[n++] = Y4;
			OutBuff[u++] = (U + U2) / 2;
			OutBuff[v++] = (V + V2) / 2;
		}
	}
}
#endif

static int is_valid_v4l2dev(v4l2dev device)
{
	if(!device) return 0;
	if(device->fd<0) return 0;
	if(device->n_valid_buffers<1) return 0;
	if(!device->mmap_buffers) return 0;
	return 1;
}

/**
 * @brief Open a V4L2 device and return a handle object.
 * @param device_node File path to the device node.
 */
v4l2dev v4l2dev_open(const char* device_node)
{
	v4l2dev device = NULL;
	int fd = 0;

	if(!device_node) return NULL;

	fd = open(device_node,  O_RDWR, 0);

	if(fd == -1) return NULL;

	device = calloc(1, sizeof(struct v4l2dev));
	device->fd = fd;

	return device;
}

/**
 * @brief Initialize the v4l2dev object and start capturing.
 * @details This function will set the device into specified capture mode, and map buffers to userspace memory. If all things were done, start capturing.
 * @param device Opened v4l2dev object.
 * @param width
 * @param height
 * @param n_buffers Number of buffers.
 */
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

	result = ioctl(device->fd, VIDIOC_S_INPUT, &input_index);


	/* Set pixel format */

	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width	= width;
	fmt.fmt.pix.height	= height;
	fmt.fmt.pix.pixelformat	= V4L2_PIX_FMT_YUYV;

	if(-1 == ioctl(device->fd, VIDIOC_S_FMT, &fmt))
	{
		fprintf(stderr, "Unsupported pixel format!\n");
	}


	/* Get real pixel format */

	ioctl(device->fd, VIDIOC_G_FMT, &fmt);
	device->width	= fmt.fmt.pix.width;
	device->height	= fmt.fmt.pix.height;


	memset(&req, 0, sizeof(struct v4l2_requestbuffers));
	req.count               = n_buffers;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;
	ioctl(device->fd, VIDIOC_REQBUFS, &req);

	device->mmap_buffers = calloc(n_buffers, sizeof(unsigned char*));

	for(i = 0;i < n_buffers;i++)
	{
		memset(&buf, 0, sizeof(struct v4l2_buffer));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;

		result = ioctl(device->fd, VIDIOC_QUERYBUF, &buf);
		device->buffer_size = buf.length;
		device->mmap_buffers[i] = mmap (NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED,device->fd, buf.m.offset);

		if(MAP_FAILED == device->mmap_buffers[i]) break;
	}

	device->n_valid_buffers = i;

	device->buffer = calloc(1, device->width * device->height * 3 / 2);

	for(i = 0;i < device->n_valid_buffers;i++)
	{
		memset(&buf, 0, sizeof(struct v4l2_buffer));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;

		result = ioctl(device->fd, VIDIOC_QBUF, &buf);
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(device->fd, VIDIOC_STREAMON, &type);

#ifndef SOFTWARE_YUV422_TO_YUV420

	/* Setup hardware accelerated YUYV to I420 conversion and IPU display */
	ipu_init(device->width, device->height, IPU_PIX_FMT_YUYV, device->width, device->height, IPU_PIX_FMT_YUV420P, 0);
#endif
}

/**
 * @brief Get output buffer size.
 * @details Call this function after v4l2dev_init() to get the output buffer size, because the resolution you specified may not match to any one that the device supports.
 */
size_t v4l2dev_get_buffersize(v4l2dev device)
{
	if(!is_valid_v4l2dev(device)) return 0;
	return device->buffer_size*3/4;
}

/**
 * @brief Close a V4L2 device and free all resource.
 */
void v4l2dev_close(v4l2dev* device)
{
	enum v4l2_buf_type type;
	int i;
	v4l2dev ptr = *device;

	if(!device) return;
	if(!ptr) return;


	/* Stop capturing */

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(ptr->fd, VIDIOC_STREAMOFF, &type);


	/* Unmap buffers */

	if(ptr->n_valid_buffers)
	{
		for(i = 0;i< ptr->n_valid_buffers;++i)
		{
			munmap(ptr->mmap_buffers[i], ptr->buffer_size);
		}
	}

#ifndef SOFTWARE_YUV422_TO_YUV420
	ipu_uninit();
#endif

	/* Free memory */

	if(ptr->mmap_buffers) free(ptr->mmap_buffers);
	if(ptr->buffer) free(ptr->buffer);
	if(ptr->fd >=0) close(ptr->fd);
	free(ptr);
	*device = NULL;
}

/**
 * @brief Read a frame from V4L2 device. The output pixel format is I420.
 * @param device Target V4L2 device
 * @return Pointer to the memory mapped RAW buffer
 */
const unsigned char* v4l2dev_read(v4l2dev device)
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

		result = ioctl(device->fd, VIDIOC_DQBUF, &buf);

		if(result == -1)
		{
			if(errno == EAGAIN)
			{
				fprintf(stderr, "Waiting...\n");
				continue;
			}
		}

		result = ioctl(device->fd, VIDIOC_QBUF, &buf);
#ifdef	SOFTWARE_YUV422_TO_YUV420
		convert_yuv422_to_yuv420(device->mmap_buffers[buf.index], device->buffer, device->width, device->height);
#else
		ipu_buffer_update(device->mmap_buffers[buf.index], device->buffer);
#endif
		return device->buffer;
	}
	return NULL;
}

