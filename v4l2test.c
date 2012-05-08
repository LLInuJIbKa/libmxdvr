#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "platform.h"
#include "v4l2dev.h"
#include "mxc_ipu.h"
#include "font.h"

#define DEVICE_NODE	"/dev/video0"
#define TTY_PATH	"/dev/tty0"

#define CAPTURE_WIDTH	(640)
#define CAPTURE_HEIGHT	(360)

#define DISPLAY_WIDTH	(1024)
#define DISPLAY_HEIGHT	(576)

#define FRAME_PER_SECOND	(25)

int main(int argc, char **argv)
{
	v4l2dev device = NULL;
	text_layout text = NULL;
	unsigned char* buffer = NULL;
	int i;
	unsigned char* rgb_buffer = NULL;
	size_t buffersize;
	ipu_lib_handle_t* ipu_handle = NULL;
	char timestring[256] = { };
	time_t rawtime;
	struct tm* timeinfo;

	fb_wakeup(TTY_PATH);
	tty_set_cursor_visible(TTY_PATH, 0);

	rgb_buffer = calloc(1, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);

	device = v4l2dev_open(DEVICE_NODE);
	v4l2dev_init(device, CAPTURE_WIDTH, CAPTURE_HEIGHT, 4);

	buffersize = v4l2dev_get_buffersize(device);
	ipu_query_task();
	ipu_handle = ipu_init(CAPTURE_WIDTH, CAPTURE_HEIGHT, IPU_PIX_FMT_YUV420P, DISPLAY_WIDTH, DISPLAY_HEIGHT, IPU_PIX_FMT_RGB565, 1);

	text = text_layout_create(280, 30);
	text_layout_set_font(text, "Liberation Mono", 24);

	for(i = 0; i < FRAME_PER_SECOND * 30; ++i)
	{
		buffer = v4l2dev_read(device);
		if((i % FRAME_PER_SECOND) == 0)
		{
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(timestring, 255, "%p %l:%m:%S %Y/%m/%d", timeinfo);
			text_layout_render_markup_text(text, timestring);
		}
		text_layout_copy_to_yuv420p(text, 360, 300, buffer, CAPTURE_WIDTH, CAPTURE_HEIGHT);
		ipu_buffer_update(ipu_handle, buffer, rgb_buffer);
	}
	text_layout_destroy(text);
	ipu_uninit(&ipu_handle);
	v4l2dev_close(&device);

	return EXIT_SUCCESS;
}

