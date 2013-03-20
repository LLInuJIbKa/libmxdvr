/**
 * @file v4l2dev.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 * @brief Video4Linux2 device module
 * @details This module provides functions of the v4l2dev object. Users can use these APIs to read image data from V4L2 devices.
 */
#ifndef V4L2DEV_H_
#define V4L2DEV_H_

#include <linux/videodev2.h>
#include "queue.h"


/**
 * @brief Handle object of V4L2 devices
 */
typedef struct v4l2dev* v4l2dev;

#define DUMMY_V4L2_DEVICE_PATH	"dummy"
#define DUMMY_V4L2_DEVICE_FPS	(31)

enum V4L2_pixelformat
{
	RAW,
	MJPEG,
	H264
};


/**
 * @brief Open a V4L2 device and return a handle object.
 * @param device_node File path to the device node.
 */
v4l2dev v4l2dev_open(const char* device_node);

/**
 * @brief Initialize the v4l2dev object and start capturing.
 * @details This function will set the device into specified capture mode, and map buffers to userspace memory. If all things were done, start capturing.
 * @param device Opened v4l2dev object.
 * @param width
 * @param height
 * @param n_buffers Number of buffers.
 */
void v4l2dev_init(v4l2dev device, const enum V4L2_pixelformat format, const int width, const int height, const int n_buffers);

/**
 * @brief Close a V4L2 device and free all resource.
 */
void v4l2dev_close(v4l2dev* device);

/**
 * @brief Get output buffer size.
 * @details Call this function after v4l2dev_init() to get the output buffer size, because the resolution you specified may not match to any one that the device supports.
 */
size_t v4l2dev_get_buffersize(v4l2dev device);

/**
 * @brief Read a frame from V4L2 device. The default output pixel format is yuv420p.
 * @param device Target V4L2 device
 * @param output Pointer to the output buffer
 * @return Number of bytes read
 */
int v4l2dev_read(v4l2dev device, unsigned char* output);

/**
 * @brief Start enqueuing thread.
 * @param device Target V4L2 device
 */
void v4l2dev_start_enqueuing(v4l2dev device);

/**
 * @brief Stop enqueuing thread.
 * @param device Target V4L2 device
 */
void v4l2dev_stop_enqueuing(v4l2dev device);

/**
 * @brief Get the queue object.
 * @param device Target V4L2 device
 */
queue v4l2dev_get_queue(v4l2dev device);

#endif /* V4L2DEV_H_ */
