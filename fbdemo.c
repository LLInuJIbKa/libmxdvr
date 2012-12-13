#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "android_fbclient.h"
#include "mxc_vpu.h"
#include "platform.h"

#define TTY_PATH	"/dev/tty0"

int main(int argc, char **argv)
{
	char cmd[256] = {};
	int isLaunched = 0;

	fb_wakeup(TTY_PATH);
	vpu_init();

	android_prober_start();

	while(1)
	{

		fgets(cmd, 255, stdin);

		if(!android_fbclient_is_running())
			isLaunched = 0;

		if(!strcmp(cmd, "start\n"))
		{
			if(!isLaunched && android_is_device_connected())
			{
				android_fbserver_start();
				fputs("fbserver started\n", stderr);
				sleep(1);
				android_fbclient_start();
				fputs("fbclient started\n", stderr);
				isLaunched = 1;
			}else
			{
				if(isLaunched)
					fputs("fbserver has already started\n", stderr);
				else
					fputs("Device not found\n", stderr);
			}
		}


		if(!strcmp(cmd, "stop\n"))
		{
			if(isLaunched)
			{
				android_fbclient_stop();
				android_fbserver_stop();
				isLaunched = 0;
			}else
			{
				fputs("Not launched yet\n", stderr);
			}

		}

		if(!strcmp(cmd, "exit\n"))
			break;
	}

	scanf("%*c");

	if(isLaunched)
	{
		android_fbclient_stop();
		android_fbserver_stop();
	}

	vpu_uninit();

	return EXIT_SUCCESS;
}
