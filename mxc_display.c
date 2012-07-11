/*
 * Copyright 2004-2010 Freescale Semiconductor, Inc.
 *
 * Copyright (c) 2006, Chips & Media.  All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "mxc_display.h"
#include "mxc_defs.h"
#include <linux/mxc_v4l2.h>

#define V4L2_MXC_ROTATE_NONE                    0
#define V4L2_MXC_ROTATE_VERT_FLIP               1
#define V4L2_MXC_ROTATE_HORIZ_FLIP              2
#define V4L2_MXC_ROTATE_180                     3
#define V4L2_MXC_ROTATE_90_RIGHT                4
#define V4L2_MXC_ROTATE_90_RIGHT_VFLIP          5
#define V4L2_MXC_ROTATE_90_RIGHT_HFLIP          6
#define V4L2_MXC_ROTATE_90_LEFT                 7

static __inline int queue_size(struct ipu_queue * q)
{
        if (q->tail >= q->head)
                return (q->tail - q->head);
        else
                return ((q->tail + QUEUE_SIZE) - q->head);
}

static __inline int queue_buf(struct ipu_queue * q, int idx)
{
        if (((q->tail + 1) % QUEUE_SIZE) == q->head)
                return -1;      /* queue full */
        q->list[q->tail] = idx;
        q->tail = (q->tail + 1) % QUEUE_SIZE;
        return 0;
}

static __inline int dequeue_buf(struct ipu_queue * q)
{
        int ret;
        if (q->tail == q->head)
                return -1;      /* queue empty */
        ret = q->list[q->head];
        q->head = (q->head + 1) % QUEUE_SIZE;
        return ret;
}

static __inline int peek_next_buf(struct ipu_queue * q)
{
        if (q->tail == q->head)
                return -1;      /* queue empty */
        return q->list[q->head];
}

int ipu_memory_alloc(int size, int cnt, dma_addr_t paddr[], void * vaddr[], int fd_fb_alloc)
{
	int i, ret = 0;

	for (i=0;i<cnt;i++) {
		/*alloc mem from DMA zone*/
		/*input as request mem size */
		paddr[i] = size;
		if ( ioctl(fd_fb_alloc, FBIO_ALLOC, &(paddr[i])) < 0) {
			printf("Unable alloc mem from /dev/fb0\n");
			close(fd_fb_alloc);
			ret = -1;
			goto done;
		}

		vaddr[i] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
				fd_fb_alloc, paddr[i]);
		if (vaddr[i] == MAP_FAILED) {
			printf("mmap failed!\n");
			ret = -1;
			goto done;
		}
	}
done:
	return ret;
}

void ipu_memory_free(int size, int cnt, dma_addr_t paddr[], void * vaddr[], int fd_fb_alloc)
{
	int i;

	for (i=0;i<cnt;i++) {
		if (vaddr[i])
			munmap(vaddr[i], size);
		if (paddr[i])
			ioctl(fd_fb_alloc, FBIO_FREE, &(paddr[i]));
	}
}

static pthread_mutex_t ipu_mutex;
static pthread_cond_t ipu_cond;
static int ipu_waiting = 0;
static int ipu_running = 0;
static inline void wait_queue()
{
	pthread_mutex_lock(&ipu_mutex);
	ipu_waiting = 1;
	pthread_cond_wait(&ipu_cond, &ipu_mutex);
	pthread_mutex_unlock(&ipu_mutex);
}

static inline void wakeup_queue()
{
	pthread_cond_signal(&ipu_cond);
}

int quitflag;
int vpu_v4l_performance_test;

static pthread_mutex_t v4l_mutex;

/* The thread for display in performance test with v4l */
void v4l_disp_loop_thread(void *arg)
{
	struct DecodingInstance *dec = (struct DecodingInstance *)arg;
	struct vpu_display *disp = dec->disp;
	pthread_attr_t attr;
	struct timeval ts;
	int error_status = 0, ret;
	struct v4l2_buffer buffer;

	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);

	while (!error_status && !quitflag) {
		/* Use timed wait here */
		do {
			gettimeofday(&ts, NULL);
			ts.tv_usec +=100000; // 100ms

		} while ((sem_timedwait(&disp->avaiable_dequeue_frame, (struct timespec*)&ts) != 0) && !quitflag);
		if (quitflag)
			break;

		buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buffer.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(disp->fd, VIDIOC_DQBUF, &buffer);
		if (ret < 0) {
			fputs("VIDIOC_DQBUF failed\n", stderr);
			error_status = 1;
		}
		/* Clear the flag after showing */
		ret = vpu_DecClrDispFlag(dec->handle, buffer.index);

		pthread_mutex_lock(&v4l_mutex);
		disp->queued_count--;
		pthread_mutex_unlock(&v4l_mutex);
		sem_post(&disp->avaiable_decoding_frame);
	}
	pthread_attr_destroy(&attr);
	return;
}

void ipu_disp_loop_thread(void *arg)
{
	struct DecodingInstance *dec = (struct DecodingInstance *)arg;
	struct vpu_display *disp = dec->disp;
	int index = -1, disp_clr_index, tmp_idx[3] = {0,0,0}, err, mode;
	pthread_attr_t attr;

	ipu_running = 1;

	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);

	while(1) {
		disp_clr_index = index;
		index = dequeue_buf(&(disp->ipu_q));
		if (index < 0) {
			wait_queue();
			ipu_waiting = 0;
			index = dequeue_buf(&(disp->ipu_q));
			if (index < 0) {
				fputs("thread is going to finish\n", stderr);
				break;
			}
		}

		if (disp->ncount == 0) {
			disp->input.user_def_paddr[0] = disp->ipu_bufs[index].ipu_paddr;
			/* For video de-interlace, Low/Medium motion */
			tmp_idx[0] = index;
		}else if ((disp->ncount == 1)) {
			disp->input.user_def_paddr[disp->ncount] = disp->ipu_bufs[index].ipu_paddr;
			mode = (OP_STREAM_MODE | TASK_PP_MODE);
			err = mxc_ipu_lib_task_init(&(disp->input), NULL, &(disp->output), mode, &(disp->ipu_handle));
			if (err < 0) {
				fprintf(stderr, "mxc_ipu_lib_task_init failed, err %d\n", err);
				quitflag = 1;
				return;
			}
			/* it only enable ipu task and finish first frame */
			err = mxc_ipu_lib_task_buf_update(&(disp->ipu_handle), 0, 0, 0, NULL, NULL);
			if (err < 0) {
				fprintf(stderr, "mxc_ipu_lib_task_buf_update failed, err %d\n", err);
				quitflag = 1;
				break;
			}
		} else {
			err = mxc_ipu_lib_task_buf_update(&(disp->ipu_handle), disp->ipu_bufs[index].ipu_paddr,
					0, 0, NULL, NULL);
			if (err < 0) {
				fprintf(stderr, "mxc_ipu_lib_task_buf_update failed, err %d\n", err);
				quitflag = 1;
				break;
			}

		}

		disp->ncount++;
	}
	mxc_ipu_lib_task_uninit(&(disp->ipu_handle));
	pthread_attr_destroy(&attr);
	fputs("Disp loop thread exit\n", stderr);
	ipu_running = 0;
	return;
}

struct vpu_display *
ipu_display_open(struct DecodingInstance *dec, int nframes, int w, int h, int x, int y)
{
	int width = dec->picwidth;
	int height = dec->picheight;
	int disp_width = w;
	int disp_height = h;
	int disp_left =  x;
	int disp_top =  y;
	char motion_mode = 0;
	int err = 0, i;
	struct vpu_display *disp;
	struct mxcfb_gbl_alpha alpha;
	struct fb_var_screeninfo fb_var;

	disp = (struct vpu_display *)calloc(1, sizeof(struct vpu_display));
	if (disp == NULL) {
		fputs("falied to allocate vpu_display\n", stderr);
		return NULL;
	}

	/* set alpha */
#ifdef BUILD_FOR_ANDROID
	disp->fd = open("/dev/graphics/fb0", O_RDWR, 0);
#else
	disp->fd = open("/dev/fb0", O_RDWR, 0);
#endif
	if (disp->fd < 0) {
		fputs("unable to open fb0\n", stderr);
		free(disp);
		return NULL;
	}
	alpha.alpha = 0;
	alpha.enable = 1;
	if (ioctl(disp->fd, MXCFB_SET_GBL_ALPHA, &alpha) < 0) {
		fputs("set alpha blending failed\n", stderr);
		close(disp->fd);
		free(disp);
		return NULL;
	}
	if ( ioctl(disp->fd, FBIOGET_VSCREENINFO, &fb_var) < 0) {
		fputs("Get FB var info failed!\n", stderr);
		close(disp->fd);
		free(disp);
		return NULL;
	}
	if (!disp_width || !disp_height) {
		disp_width = fb_var.xres;
		disp_height = fb_var.yres;
	}

	/* allocate buffers, use an extra buf for init buf */
	disp->nframes = nframes;
	disp->frame_size = width*height*3/2;
	for (i=0;i<nframes;i++) {
		err = ipu_memory_alloc(disp->frame_size, 1, &(disp->ipu_bufs[i].ipu_paddr),
				&(disp->ipu_bufs[i].ipu_vaddr), disp->fd);
		if ( err < 0) {
			fputs("ipu_memory_alloc failed\n", stderr);
			free(disp);
			return NULL;
		}
	}

	memset(&(disp->ipu_handle), 0, sizeof(ipu_lib_handle_t));
	memset(&(disp->input), 0, sizeof(ipu_lib_input_param_t));
	memset(&(disp->output), 0, sizeof(ipu_lib_output_param_t));

        disp->input.width = width;
        disp->input.height = height;
	disp->input.input_crop_win.pos.x = 0;
	disp->input.input_crop_win.pos.y = 0;
	disp->input.input_crop_win.win_w = 0;
	disp->input.input_crop_win.win_h = 0;

	/* Set VDI motion algorithm. */
	if (motion_mode) {
		if (motion_mode == 'h') {
			disp->input.motion_sel = HIGH_MOTION;
		} else if (motion_mode == 'l') {
			disp->input.motion_sel = LOW_MOTION;
		} else if (motion_mode == 'm') {
			disp->input.motion_sel = MED_MOTION;
		} else {
			disp->input.motion_sel = MED_MOTION;
			fprintf(stderr, "%c unknown motion mode, medium, the default is used\n", motion_mode);
		}
	}

	disp->input.fmt = V4L2_PIX_FMT_YUV420;
	disp->output.width = disp_width;
	disp->output.height = disp_height;
	disp->output.fmt = V4L2_PIX_FMT_UYVY;
	disp->output.fb_disp.pos.x = disp_left;
	disp->output.fb_disp.pos.y = disp_top;
	disp->output.show_to_fb = 1;
	disp->output.fb_disp.fb_num = 2;

	fprintf(stderr, "Display to %d %d, top offset %d, left offset %d\n",
			disp_width, disp_height, disp_top, disp_left);

	disp->ipu_q.tail = disp->ipu_q.head = 0;
	disp->stopping = 0;

	dec->disp = disp;
	pthread_mutex_init(&ipu_mutex, NULL);
	pthread_cond_init(&ipu_cond, NULL);

	/* start disp loop thread */
	pthread_create(&(disp->ipu_disp_loop_thread), NULL, (void*)ipu_disp_loop_thread, (void *)dec);

	return disp;
}

void ipu_display_close(struct vpu_display *disp)
{
	int i;

	disp->stopping = 1;
	disp->deinterlaced = 0;
	while(ipu_running && ((queue_size(&(disp->ipu_q)) > 0) || !ipu_waiting)) usleep(10000);
	if (ipu_running) {
		wakeup_queue();
		fputs("Join disp loop thread\n", stderr);
		pthread_join(disp->ipu_disp_loop_thread, NULL);
	}
	pthread_mutex_destroy(&ipu_mutex);
	pthread_cond_destroy(&ipu_cond);
	for (i=0;i<disp->nframes;i++)
		ipu_memory_free(disp->frame_size, 1, &(disp->ipu_bufs[i].ipu_paddr),
				&(disp->ipu_bufs[i].ipu_vaddr), disp->fd);
	close(disp->fd);
	free(disp);
}

int ipu_put_data(struct vpu_display *disp, int index, int field, int fps)
{
	/*TODO: ipu lib dose not support fps control yet*/

	disp->ipu_bufs[index].field = field;
	if (field == V4L2_FIELD_TOP || field == V4L2_FIELD_BOTTOM ||
	    field == V4L2_FIELD_INTERLACED_TB ||
	    field == V4L2_FIELD_INTERLACED_BT)
		disp->deinterlaced = 1;
	queue_buf(&(disp->ipu_q), index);
	wakeup_queue();

	return 0;
}

void v4l_free_bufs(int n, struct vpu_display *disp)
{
	int i;
	struct v4l_buf *buf;
	for (i = 0; i < n; i++) {
		if (disp->buffers[i] != NULL) {
			buf = disp->buffers[i];
			if (buf->start > 0)
				munmap(buf->start, buf->length);

			free(buf);
			disp->buffers[i] = NULL;
		}
	}
}

struct vpu_display* v4l_display_open(struct DecodingInstance *dec, int nframes, int w, int h, int x, int y)
{

	int width = dec->picwidth;
	int height = dec->picheight;

	int fd = -1, err = 0, out = 0, i = 0;
	char v4l_device[32] = "/dev/video16";
	struct v4l2_format fmt = {};
	struct v4l2_requestbuffers reqbuf = {};
	struct vpu_display *disp;
	int fd_fb;
	char *tv_mode;
	char motion_mode = 0;
	struct mxcfb_gbl_alpha alpha;

	if (cpu_is_mx27()) {
		out = 0;
	} else {
		out = 3;
#ifdef BUILD_FOR_ANDROID
		fd_fb = open("/dev/graphics/fb0", O_RDWR, 0);
#else
		fd_fb = open("/dev/fb0", O_RDWR, 0);
#endif
		if (fd_fb < 0) {
			fputs("unable to open fb0\n", stderr);
			return NULL;
		}
		alpha.alpha = 0;
		alpha.enable = 1;
		if (ioctl(fd_fb, MXCFB_SET_GBL_ALPHA, &alpha) < 0) {
			fputs("set alpha blending failed\n", stderr);
			return NULL;
		}
		close(fd_fb);
	}

	tv_mode = getenv("VPU_TV_MODE");

	if (tv_mode) {
		err = system("/bin/echo 1 > /sys/class/graphics/fb1/blank");
		if (!strcmp(tv_mode, "NTSC")) {
			err = system("/bin/echo U:720x480i-60 > /sys/class/graphics/fb1/mode");
			out = 5;
		} else if (!strcmp(tv_mode, "PAL")) {
			err = system("/bin/echo U:720x576i-50 > /sys/class/graphics/fb1/mode");
			out = 5;
		} else if (!strcmp(tv_mode, "720P")) {
			err = system("/bin/echo U:1280x720p-60 > /sys/class/graphics/fb1/mode");
			out = 5;
		} else {
			out = 3;
			fputs("VPU_TV_MODE should be set to NTSC, PAL, or 720P.\n"
				 "\tDefault display is LCD if not set this environment "
				 "or set wrong string.\n", stderr);
		}
		err = system("/bin/echo 0 > /sys/class/graphics/fb1/blank");
		if (err == -1)
		{
			fputs("set tv mode error\n", stderr);
		}
		/* make sure tvout init done */
		sleep(2);
	}

	disp = (struct vpu_display *)calloc(1, sizeof(struct vpu_display));
       	if (disp == NULL) {
       		fputs("falied to allocate vpu_display\n", stderr);
		return NULL;
	}

	fd = open(v4l_device, O_RDWR, 0);
	if (fd < 0) {
		fprintf(stderr, "unable to open %s\n", v4l_device);
		goto err;
	}

	err = ioctl(fd, VIDIOC_S_OUTPUT, &out);
	if (err < 0) {
		fputs("VIDIOC_S_OUTPUT failed\n", stderr);
		goto err;
	}

	/* Set VDI motion algorithm. */
	if (motion_mode) {
		struct v4l2_control ctrl;
		ctrl.id = V4L2_CID_MXC_MOTION;
		if (motion_mode == 'h') {
			ctrl.value = HIGH_MOTION;
		} else if (motion_mode == 'l') {
			ctrl.value = LOW_MOTION;
		} else if (motion_mode == 'm') {
			ctrl.value = MED_MOTION;
		} else {
			ctrl.value = MED_MOTION;
		}
		err = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
		if (err < 0) {
			fputs("VIDIOC_S_CTRL failed\n", stderr);
			goto err;
		}
	}


	struct v4l2_control ctrl;
	ctrl.id = V4L2_CID_PRIVATE_BASE;
	ctrl.value = 0;
	err = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
	if (err < 0) {
		fputs("VIDIOC_S_CTRL failed\n", stderr);
		goto err;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	/*
	 * Just consider one case:
	 * (top,left) = (0,0)
	 */

	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.bytesperline = width;

	dprintf(1, "fmt.fmt.pix.width = %d\n\tfmt.fmt.pix.height = %d\n",
				fmt.fmt.pix.width, fmt.fmt.pix.height);

	fmt.fmt.pix.field = V4L2_FIELD_ANY;

	if(dec->mjpg_fmt == MODE420)
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	else if(dec->mjpg_fmt == MODE422)
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
	else
	{
		goto err;
	}

	err = ioctl(fd, VIDIOC_S_FMT, &fmt);
	if (err < 0) {
		goto err;
	}

	err = ioctl(fd, VIDIOC_G_FMT, &fmt);
	if (err < 0) {
		goto err;
	}

	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = nframes;

	err = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	if (err < 0) {
		goto err;
	}

	if (reqbuf.count < nframes) {
		goto err;
	}

	for (i = 0; i < nframes; i++) {
		struct v4l2_buffer buffer = {0};
		struct v4l_buf *buf;

		buf = calloc(1, sizeof(struct v4l_buf));
		if (buf == NULL) {
			v4l_free_bufs(i, disp);
			goto err;
		}

		disp->buffers[i] = buf;

		buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		err = ioctl(fd, VIDIOC_QUERYBUF, &buffer);
		if (err < 0) {
			v4l_free_bufs(i, disp);
			goto err;
		}

		buf->offset = buffer.m.offset;
		buf->length = buffer.length;
		dprintf(3, "V4L2buf phy addr: %08x, size = %d\n",
					(unsigned int)buf->offset, buf->length);
		buf->start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, buffer.m.offset);

		if (buf->start == MAP_FAILED) {
			v4l_free_bufs(i, disp);
			goto err;
		}

	}

	disp->fd = fd;
	disp->nframes = nframes;

	/*
	 * Use environment VIDEO_PERFORMANCE_TEST to select different mode.
	 * When doing performance test, video decoding and display are in different
	 * threads and default display fps is controlled by cmd. Display will
	 * show the frame immediately if user doesn't input fps with -a option.
	 * This is different from normal unit test.
	 */

	return disp;
err:
	close(fd);
	free(disp);
	return NULL;
}

void v4l_display_close(struct vpu_display *disp)
{
	int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if (disp) {

		while (disp->queued_count > 0) {
			disp->buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			disp->buf.memory = V4L2_MEMORY_MMAP;
			if (ioctl(disp->fd, VIDIOC_DQBUF, &disp->buf) < 0)
				break;
			disp->queued_count--;
		}
		ioctl(disp->fd, VIDIOC_STREAMOFF, &type);
		v4l_free_bufs(disp->nframes, disp);
		close(disp->fd);
		free(disp);
	}
}

int v4l_put_data(struct vpu_display *disp, int index, int field, int fps)
{
	struct timeval tv;
	int err, type, threshold;
	struct v4l2_format fmt = {0};

	if (disp->ncount == 0) {
		gettimeofday(&tv, 0);
		disp->buf.timestamp.tv_sec = tv.tv_sec;
		disp->buf.timestamp.tv_usec = tv.tv_usec;

		disp->sec = tv.tv_sec;
		disp->usec = tv.tv_usec;
	}

	disp->buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	disp->buf.memory = V4L2_MEMORY_MMAP;

	/* query buffer info */
	disp->buf.index = index;
	err = ioctl(disp->fd, VIDIOC_QUERYBUF, &disp->buf);
	if (err < 0) {
		goto err;
	}

	if (disp->ncount > 0) {
		if (fps != 0) {
			disp->usec += (1000000 / fps);
			if (disp->usec >= 1000000) {
				disp->sec += 1;
				disp->usec -= 1000000;
			}

			disp->buf.timestamp.tv_sec = disp->sec;
			disp->buf.timestamp.tv_usec = disp->usec;
		} else {
			gettimeofday(&tv, 0);
			disp->buf.timestamp.tv_sec = tv.tv_sec;
			disp->buf.timestamp.tv_usec = tv.tv_usec;
		}
	}

	disp->buf.index = index;
	disp->buf.field = field;

	err = ioctl(disp->fd, VIDIOC_QBUF, &disp->buf);
	if (err < 0) {
		goto err;
	}

	disp->queued_count++;

	if (disp->ncount == 1) {
		if ((disp->buf.field == V4L2_FIELD_TOP) ||
		    (disp->buf.field == V4L2_FIELD_BOTTOM) ||
		    (disp->buf.field == V4L2_FIELD_INTERLACED_TB) ||
		    (disp->buf.field == V4L2_FIELD_INTERLACED_BT)) {
			/* For interlace feature */
			fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			err = ioctl(disp->fd, VIDIOC_G_FMT, &fmt);
			if (err < 0) {
				goto err;
			}
			if ((disp->buf.field == V4L2_FIELD_TOP) ||
			    (disp->buf.field == V4L2_FIELD_BOTTOM))
				fmt.fmt.pix.field = V4L2_FIELD_ALTERNATE;
			else
				fmt.fmt.pix.field = field;
			fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			err = ioctl(disp->fd, VIDIOC_S_FMT, &fmt);
			if (err < 0) {
				goto err;
			}
		}
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		err = ioctl(disp->fd, VIDIOC_STREAMON, &type);
		if (err < 0) {
			goto err;
		}
	}

	disp->ncount++;

	threshold = 2;
	if (disp->buf.field == V4L2_FIELD_ANY || disp->buf.field == V4L2_FIELD_NONE)
		threshold = 1;
	if (disp->queued_count > threshold) {

		disp->buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		disp->buf.memory = V4L2_MEMORY_MMAP;
		err = ioctl(disp->fd, VIDIOC_DQBUF, &disp->buf);
		if (err < 0) {
			goto err;
		}
		disp->queued_count--;

	}
	else
		disp->buf.index = -1;

	return 0;

err:
	return -1;
}

