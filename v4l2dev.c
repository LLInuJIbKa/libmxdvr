#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "v4l2dev.h"


struct v4l2dev
{
	int	fd;
};



/**
 * @brief Open a V4L2 device and return a handle object.
 * @param device_node	File path to the device node.
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
 * @brief Initialize the v4l2dev object.
 * @param device Opened v4l2dev object.
 * @param width
 * @param height
 * @param pixel_fmt Pixel format. For instance: V4L2_PIX_FMT_YUYV
 * @param n_buffers Number of buffers.
 */
void v4l2dev_init(v4l2dev device, const int width, const int height, const int pixel_fmt, const int n_buffers)
{
	int input_index = 0;
	struct v4l2_format fmt;

	if(!device) return;

	/* Select input 0 */

	ioctl(device->fd, VIDIOC_S_INPUT, &input_index);


	/* Set pixel format */

	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width	= width;
	fmt.fmt.pix.height	= height;
	fmt.fmt.pix.pixelformat	= pixel_fmt;

	if(-1 == ioctl(device->fd, VIDIOC_S_FMT, &fmt))
	{
		fprintf(stderr, "Unsupported pixel format!\n");
	}

}

/**
 * @brief Close a V4L2 device and free all resource.
 */
void v4l2dev_close(v4l2dev* device)
{
	v4l2dev ptr = *device;

	if(!device) return;
	if(!ptr) return;

	if(ptr->fd >=0) close(ptr->fd);
	free(ptr);
	*device = NULL;
}

