#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "fbclient.h"
#include "mxc_vpu.h"

int main(int argc, char **argv)
{
	fd_set fds;
	struct timeval tv = { 0L, 0L };

	vpu_init();
	fbclient_start();

	while(1)
	{
		usleep(30000);

		/* Detect ENTER key */
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		if(select(1, &fds, NULL, NULL, &tv) > 0) break;
	}


	/* Consume stdin */

	scanf("%*c");
	fputs("Quiting ...\n", stderr);

	fbclient_stop();
	vpu_uninit();


	return EXIT_SUCCESS;
}
