#ifndef V4L2DEV_H_
#define V4L2DEV_H_


typedef struct v4l2dev* v4l2dev;

v4l2dev v4l2dev_open(const char* device_node);
void v4l2dev_init(v4l2dev device, const int width, const int height, const int pixel_fmt, const int n_buffers);
int v4l2dev_read(v4l2dev device, unsigned char** const mmap );
void v4l2dev_close(v4l2dev* device);



#endif /* V4L2DEV_H_ */
