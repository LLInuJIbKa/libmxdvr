#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "fbclient.h"



int main(int argc, char **argv)
{
	fd_set fds;
	struct timeval tv = { 0L, 0L };

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

	fbclient_stop();

	return EXIT_SUCCESS;
}
