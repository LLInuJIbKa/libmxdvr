#include <stdio.h>
#include <stdlib.h>
#include "v4l2dev.h"

#define DEVICE_NODE	"/dev/video0"

int main(int argc, char **argv)
{
	FILE* fp = NULL;
	v4l2dev device = NULL;
	const unsigned char* buffer = NULL;
	int i;
	size_t buffersize;


	device = v4l2dev_open(DEVICE_NODE);
	v4l2dev_init(device, 640, 360, 4);
	buffersize = v4l2dev_get_buffersize(device);

	fp = fopen("test.yuv", "w");

	for(i=0;i<30*10;i++)
	{
		buffer = v4l2dev_read(device);
		if(!buffer) fprintf(stderr, "NULL\n");

		fwrite(buffer, buffersize, 1, fp);
	}
	fclose(fp);
	v4l2dev_close(&device);

	return EXIT_SUCCESS;
}

