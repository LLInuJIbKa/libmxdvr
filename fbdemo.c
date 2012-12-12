#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "android_fbclient.h"
#include "mxc_vpu.h"

int main(int argc, char **argv)
{
	vpu_init();
	android_fbclient_start();

	while(1)
	{
		sleep(1);
	}


	return EXIT_SUCCESS;
}
