#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "v4l2dev.h"
#include "mxc_ipu.h"

#define DEVICE_NODE	"/dev/video0"
#define TTY_PATH	"/dev/tty1"

#define CAPTURE_WIDTH	(1280)
#define CAPTURE_HEIGHT	(720)

#define DISPLAY_WIDTH	(1024)
#define DISPLAY_HEIGHT	(768)


int main(int argc, char **argv)
{
	FILE* fp = NULL;
	v4l2dev device = NULL;
	const unsigned char* buffer = NULL;
	int i;
	unsigned char* rgb_buffer = NULL;
	size_t buffersize;
	ipu_lib_handle_t* ipu_handle = NULL;

	fb_wakeup(TTY_PATH);
	tty_set_cursor_visible(TTY_PATH, 0);

	rgb_buffer = calloc(1, DISPLAY_WIDTH*DISPLAY_HEIGHT*2);

	device = v4l2dev_open(DEVICE_NODE);
	v4l2dev_init(device, CAPTURE_WIDTH, CAPTURE_HEIGHT, 4);


	buffersize = v4l2dev_get_buffersize(device);
	ipu_handle = ipu_init(CAPTURE_WIDTH, CAPTURE_HEIGHT, IPU_PIX_FMT_YUV420P, DISPLAY_WIDTH, DISPLAY_HEIGHT, IPU_PIX_FMT_RGB565, 1);


	fp = fopen("test.yuv", "w");

	for(i=0;i<30*10;i++)
	{
		buffer = v4l2dev_read(device);
		ipu_buffer_update(ipu_handle, buffer, rgb_buffer);
		//fwrite(buffer, buffersize, 1, fp);
	}

	ipu_uninit(&ipu_handle);
	fclose(fp);
	v4l2dev_close(&device);

	return EXIT_SUCCESS;
}

