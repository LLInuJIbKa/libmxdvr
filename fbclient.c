#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include "fbclient.h"
#include "queue.h"
#include "mxc_vpu.h"


#define BUFFER_SIZE	(262144)
#define CONNECT_PORT	(5555)

static pthread_t fbclient_pthread;
static int run;


int fbclient_thread(int arg)
{
	int socketfd, read_bytes;
	unsigned char* buffer;
	struct sockaddr_in address;
	int jpg_size;
	int remaining;
	queue input;
	DecodingInstance instance = NULL;
	int first = 1;


	input = queue_new(BUFFER_SIZE, 8);

	/* Create socket*/

	if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return -1;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(CONNECT_PORT);
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	bzero(&(address.sin_zero), 8);

	if(connect(socketfd, (struct sockaddr*)&address,sizeof(struct sockaddr)) == -1)
	{
		perror("connect");
		return -1;
	}


	/* Recieve jpeg images and display on screen */

	buffer = calloc(1, BUFFER_SIZE);
	jpg_size = 0;

	run = 1;
	while(run)
	{
		/* Get jpeg file size */
		jpg_size = 0;
		read_bytes = read(socketfd, &jpg_size, sizeof(int));

		if(read_bytes == 0) break;

		/* Recieve jpeg file */

		remaining = jpg_size;

		while(remaining>0)
		{
			read_bytes = read(socketfd, &(buffer[jpg_size-remaining]), remaining);
			remaining -= read_bytes;
		}


		queue_push(input, buffer);

		if(first)
		{
			instance = vpu_create_decoding_instance_for_v4l2(input);
			first = 0;

			vpu_start_decoding(instance);
		}

	}

	vpu_stop_decoding(instance);
	free(buffer);
	close(socketfd);

	return 0;
}

void fbclient_start(void)
{
	pthread_create(&fbclient_pthread, NULL, (void*)fbclient_thread, NULL);
}

void fbclient_stop(void)
{
	int ret;
	run = 0;
	pthread_join(fbclient_pthread, (void**)&ret);
}
