#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "platform.h"
#include "v4l2dev.h"
#include "mxc_vpu.h"

#define DEVICE_NODE	"/dev/video0"
#define TTY_PATH	"/dev/tty0"
#define OUTPUT_MP4_FILENAME	"test.mp4"
#define CAPTURE_WIDTH	(640)
#define CAPTURE_HEIGHT	(480)

#define DISPLAY_WIDTH	(1024)
#define DISPLAY_HEIGHT	(768)

#define FRAME_PER_SECOND	(25)


int main(int argc, char **argv)
{
	v4l2dev device = NULL;
	int i;
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	EncodingInstance encoding = NULL;
	DecodingInstance decoding = NULL;


	fb_wakeup(TTY_PATH);
	tty_set_cursor_visible(TTY_PATH, 0);

	device = v4l2dev_open(DEVICE_NODE);
	v4l2dev_init(device, CAPTURE_WIDTH, CAPTURE_HEIGHT, 4);

	vpu_init();

	puts("Press Enter key to exit ...");


	v4l2dev_start_enqueuing(device);
	encoding = vpu_create_encoding_instance(CAPTURE_WIDTH, CAPTURE_HEIGHT, OUTPUT_MP4_FILENAME);
	decoding = vpu_create_decoding_instance_for_v4l2(v4l2dev_get_queue(device));
	vpu_start_decoding(decoding);
	vpu_start_encoding(encoding, vpu_get_decode_queue(decoding));

	for(i = 0;; ++i)
	{
		usleep(30000);

		/* Detect ENTER key */
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		if(select(1, &fds, NULL, NULL, &tv) > 0) break;
	}


	/* Consume stdin */

	scanf("%*c");


	/* Clean */

	vpu_stop_encoding(encoding);
	vpu_stop_decoding(decoding);
	vpu_close_encoding_instance(&encoding);
	vpu_uninit();

	v4l2dev_stop_enqueuing(device);
	v4l2dev_close(&device);

	return EXIT_SUCCESS;
}

