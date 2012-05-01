#ifndef V4L2DEV_H_
#define V4L2DEV_H_

#include <linux/videodev2.h>

typedef struct v4l2dev* v4l2dev;

v4l2dev v4l2dev_open(const char* device_node);
void v4l2dev_init(v4l2dev device, const int width, const int height, const int n_buffers);
void v4l2dev_close(v4l2dev* device);
size_t v4l2dev_get_buffersize(v4l2dev device);
const unsigned char* v4l2dev_read(v4l2dev device);




#endif /* V4L2DEV_H_ */
