#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include "mxc_vpu.h"
#include "v4l2dev.h"
#include "platform.h"
#include "android_fbclient.h"

#define TTY_PATH		"/dev/tty0"
#define TEMP_VIDEO_OUTPUT_FILE	"tmp.mp4"
//#define ARCHIVE_INTERVAL 	(5 * 60 * 1000)
#define ARCHIVE_INTERVAL 	(30000)
#define CAPTURE_WIDTH	(1280)
#define CAPTURE_HEIGHT	(720)
#define DISPLAY_WIDTH	(1024)
#define DISPLAY_HEIGHT	(768)

#define PROMPT_STRING	"vhud> "

static const char * const vhud_cmd_table[] =
{
	"\n",
	"exit\n",
	"show phone\n",
	"hide phone\n",
};

enum vhud_cmd
{
	VHUD_CMD_NOOP = 0,
	VHUD_CMD_EXIT,
	VHUD_CMD_SHOW_PHONE,
	VHUD_CMD_HIDE_PHONE,
	NUMBER_OF_VHUD_CMD,
};

v4l2dev camera = NULL;
EncodingInstance encoder = NULL;
GMainLoop *main_loop = NULL;

static void finish_carvideo(void)
{
	char cmd[256] = {};
	static int i = 0;

	if(encoder)
	{
		vpu_stop_encoding(encoder);
		vpu_close_encoding_instance(&encoder);

		g_snprintf(cmd, 255, "mv tmp.mp4 %d.mp4", i++);
		system(cmd);
		g_printerr("Video archived.\n");
	}
}

static gboolean timer_callback(gpointer data)
{
	finish_carvideo();
	encoder = vpu_create_encoding_instance(CAPTURE_WIDTH, CAPTURE_HEIGHT, TEMP_VIDEO_OUTPUT_FILE);
	vpu_start_encoding(encoder, v4l2dev_get_queue(camera));

	return TRUE;
}

static gboolean kbio_callback(GIOChannel *gio, GIOCondition condition, gpointer data)
{
	char *cmd = NULL;
	unsigned int count = 0;
	enum vhud_cmd input_cmd;

	g_io_channel_read_line(gio, &cmd, &count, NULL, NULL);


	for(input_cmd = 0; input_cmd < NUMBER_OF_VHUD_CMD; ++input_cmd)
		if(!g_strcmp0(cmd, vhud_cmd_table[input_cmd]))
			break;

	switch(input_cmd)
	{
	case VHUD_CMD_EXIT:
		g_main_loop_quit(main_loop);
		break;
	case VHUD_CMD_SHOW_PHONE:

		if(!android_is_device_connected())
		{
			g_printerr("Device not found\n");
		}

		if(!android_fbclient_is_running())
		{
			android_fbserver_start();
			g_printerr("fbserver started\n");
			sleep(1);
			android_fbclient_start();
			g_printerr("fbclient started\n");
		}else
		{
			g_printerr("fbclient has already started\n");
		}


		break;
	case VHUD_CMD_HIDE_PHONE:

		if(android_fbclient_is_running())
			android_fbclient_stop();
		else
			g_printerr("fbclient is not running\n");

		break;
	case VHUD_CMD_NOOP:
		break;
	default:
		g_printerr("Unknown command.\n\n");
	}

	if(input_cmd != VHUD_CMD_EXIT)
		g_printerr(PROMPT_STRING);

	if(cmd)
		g_free(cmd);

	return TRUE;
}

int main(int argc, char **argv)
{
	GIOChannel *kbio = NULL;

	fb_wakeup(TTY_PATH);
	tty_set_cursor_visible(TTY_PATH, FALSE);


	/* Initialize glib multithread support, required if glib < 2.32 */
#ifndef GLIB_VERSION_2_32
	g_thread_init(NULL);
#endif

	/* Initialize V4L2 camera */

	camera = v4l2dev_open(DUMMY_V4L2_DEVICE_PATH);
	v4l2dev_init(camera, RAW, CAPTURE_WIDTH, CAPTURE_HEIGHT, 1);


	/* Initialize VPU for carvideo recording */

	vpu_init();



	/* Startup prober thread for detecting Android devices */

	android_prober_start();
	g_printerr("Android device prober started.\n");


	/* Setup mainloop */

	main_loop = g_main_loop_new(NULL, FALSE);
	kbio = g_io_channel_unix_new(0);
	g_io_add_watch(kbio, G_IO_IN|G_IO_HUP, kbio_callback, NULL);
	g_timeout_add(ARCHIVE_INTERVAL, timer_callback, NULL);


	/* Start carvideo recording */

	v4l2dev_start_enqueuing(camera);
	timer_callback(NULL);
	g_printerr(PROMPT_STRING);

	/* Run main loop */

	g_main_loop_run(main_loop);


	finish_carvideo();

	if(camera)
		v4l2dev_close(&camera);
	android_fbclient_stop();

	vpu_uninit();

	return EXIT_SUCCESS;
}
