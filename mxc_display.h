#ifndef MXC_DISPLAY_H_
#define MXC_DISPLAY_H_

#include <linux/videodev.h>
#include <semaphore.h>
#include <stdint.h>
#include <mxc_ipu_hl_lib.h>
#include "mxc_defs.h"

#define MAX_BUF_NUM	32
#define QUEUE_SIZE	(MAX_BUF_NUM + 1)

struct v4l_buf
{
	void *start;
	off_t offset;
	size_t length;
};

struct ipu_queue
{
	int list[MAX_BUF_NUM + 1];
	int head;
	int tail;
};

struct ipu_buf
{
	int ipu_paddr;
	void * ipu_vaddr;
	int field;
};

struct vpu_display
{
	int fd;
	int nframes;
	int ncount;
	time_t sec;
	int queued_count;
	suseconds_t usec;
	struct v4l2_buffer buf;
	struct v4l_buf *buffers[MAX_BUF_NUM];

	int frame_size;
	ipu_lib_handle_t ipu_handle;
	ipu_lib_input_param_t input;
	ipu_lib_output_param_t output;
	pthread_t ipu_disp_loop_thread;
	pthread_t v4l_disp_loop_thread;

	sem_t avaiable_decoding_frame;
	sem_t avaiable_dequeue_frame;

	struct ipu_queue ipu_q;
	struct ipu_buf ipu_bufs[MAX_BUF_NUM];
	int stopping;
	int deinterlaced;
};

struct rot
{
	int rot_en;
	int ipu_rot_en;
	int rot_angle;
};

struct vpu_display* v4l_display_open(struct DecodingInstance *dec, int nframes, int w, int h, int x, int y);
int v4l_put_data(struct vpu_display *disp, int index, int field, int fps);


#endif /* MXC_DISPLAY_H_ */
