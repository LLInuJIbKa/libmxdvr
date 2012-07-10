#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "platform.h"
#include "v4l2dev.h"
#include "mxc_ipu.h"
#include "mxc_vpu.h"
#include "font.h"
#include <time.h>

#define DEVICE_NODE	"/dev/video0"
#define TTY_PATH	"/dev/tty0"
#define OUTPUT_MP4_FILENAME	"test.mp4"
#define CAPTURE_WIDTH	(640)
#define CAPTURE_HEIGHT	(480)

#define DISPLAY_WIDTH	(1024)
#define DISPLAY_HEIGHT	(768)

#define FRAME_PER_SECOND	(25)


#define timer_start; \
{\
	struct timeval ts, te;\
	int elapsed;\
	gettimeofday(&ts, NULL);

#define timer_stop; \
	gettimeofday(&te, NULL);\
	elapsed = (te.tv_sec - ts.tv_sec) * 1000000 + (te.tv_usec - ts.tv_usec);\
	fprintf(stderr, "encoding %d ms\n", elapsed/1000);\
}




static void convert_yuv422p_to_yuv420p(unsigned char *InBuff, unsigned char *OutBuff, int width, int height)
{
	int i, j;
	unsigned char* in_u;
	unsigned char* in_v;
	unsigned char* out_u;
	unsigned char* out_v;

	/* Write Y plane */
	memcpy(OutBuff, InBuff, width * height);

	/* Write UV plane */
	for(j = 0;j < height / 2; ++j)
	{
		in_u = &(InBuff[width * height + j * 2 * width / 2]);
		in_v = &(InBuff[width * height * 3 / 2 + j * 2 * width / 2]);

		out_u = &(OutBuff[width * height + j * width / 2]);
		out_v = &(OutBuff[width * height * 5 / 4 + j * width / 2]);

		for(i = 0;i < width / 2; ++i)
		{
			out_u[i] = (in_u[i] + in_u[width / 2 + i]) / 2;
			out_v[i] = (in_v[i] + in_v[width / 2 + i]) / 2;
			out_u[i] = in_u[i];
			out_v[i] = in_v[i];

		}
	}
}





int main(int argc, char **argv)
{
	v4l2dev device = NULL;
	text_layout text = NULL;
	unsigned char* buffer = NULL;
	int i;
	unsigned char* rgb_buffer = NULL;
	ipu_lib_handle_t* ipu_handle = NULL;
	char timestring[256] = { };
	time_t rawtime;
	struct tm* timeinfo;
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	EncodingInstance encoding = NULL;
	DecodingInstance decoding = NULL;
	int ret;
	struct timeval tstart, tend;
	int elapsed;


	fb_wakeup(TTY_PATH);
	tty_set_cursor_visible(TTY_PATH, 0);

	device = v4l2dev_open(DEVICE_NODE);
	v4l2dev_init(device, CAPTURE_WIDTH, CAPTURE_HEIGHT, 4);

	rgb_buffer = calloc(1, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
	//ipu_handle = ipu_init(CAPTURE_WIDTH, CAPTURE_HEIGHT, IPU_PIX_FMT_YUV422P, DISPLAY_WIDTH, DISPLAY_HEIGHT, IPU_PIX_FMT_YUV420P, 0);
	ipu_handle = ipu_init(CAPTURE_WIDTH, CAPTURE_HEIGHT, IPU_PIX_FMT_YUV422P, CAPTURE_WIDTH, CAPTURE_HEIGHT, IPU_PIX_FMT_YUV420P, 0);
	//ipu_query_task();

	vpu_init();

	text = text_layout_create(280, 30);
	text_layout_set_font(text, "Liberation Mono", 24);

	puts("Press Enter key to exit ...");

	buffer = calloc(1, CAPTURE_WIDTH * CAPTURE_HEIGHT * 2);


	v4l2dev_start_enqueuing(device);
	encoding = vpu_create_encoding_instance(CAPTURE_WIDTH, CAPTURE_HEIGHT, OUTPUT_MP4_FILENAME);
	decoding = vpu_create_decoding_instance_for_v4l2(v4l2dev_get_queue(device));
	//vpu_start_decoding(decoding);
	//vpu_start_encoding(encoding, vpu_get_decode_queue(decoding));

	for(i = 0;; ++i)
	{

		/* Read RAW image from V4L2 device */
		ret = vpu_decode_one_frame(decoding, &rgb_buffer);

		/* Update text image every second */

		//if(i == FRAME_PER_SECOND)

		//ipu_buffer_update(ipu_handle, rgb_buffer, buffer);


		if(!ret)
		{

			{
				i = 0;
				time(&rawtime);
				timeinfo = localtime(&rawtime);
				strftime(timestring, 255, "%p %l:%M:%S %Y/%m/%d", timeinfo);
				text_layout_render_markup_text(text, timestring);
			}

			text_layout_copy_to_yuv422p(text, 360, 400, rgb_buffer, CAPTURE_WIDTH, CAPTURE_HEIGHT);
			convert_yuv422p_to_yuv420p(rgb_buffer, buffer, CAPTURE_WIDTH, CAPTURE_HEIGHT);


			//text_layout_copy_to_yuv420p(text, 360, 400, buffer, CAPTURE_WIDTH, CAPTURE_HEIGHT);

			//text_layout_copy_to_yuv422(text, 360, 400, buffer, CAPTURE_WIDTH, CAPTURE_HEIGHT);



			//ipu_buffer_update(ipu_handle, rgb_buffer, buffer);

			vpu_display(decoding);


			/* Use VPU to encode H.264 stream */
			vpu_encode_one_frame(encoding, buffer);

		}
		/* Detect ENTER key */
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		if(select(1, &fds, NULL, NULL, &tv) > 0) break;
	}





	/* Consume stdin */

	scanf("%*c");


	/* Clean */


	text_layout_destroy(text);
	ipu_uninit(&ipu_handle);

	//vpu_stop_encoding(encoding);
	//vpu_stop_decoding(decoding);

	//vpu_close_encoding_instance(&encoding);

	vpu_uninit();

	v4l2dev_stop_enqueuing(device);
	v4l2dev_close(&device);

	return EXIT_SUCCESS;
}

