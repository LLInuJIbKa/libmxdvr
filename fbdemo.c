#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fbclient.h"
#include "mxc_vpu.h"

int main(int argc, char **argv)
{
	vpu_init();
	fbclient_start();

	while(1)
	{
		sleep(1);
	}


	return EXIT_SUCCESS;
}
