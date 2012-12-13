#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <libudev.h>
#include "android_fbclient.h"
#include "queue.h"
#include "mxc_vpu.h"


#define BUFFER_SIZE	(262144)
#define CONNECT_PORT	(5555)
#define ANDROID_PROBE_INTERVAL	(500)
#define ANDROID_SERIAL_FILE	"android_serial.txt"
#define MAX_RETRY_READ_SOCKET	(10)

static pthread_t fbclient_pthread = 0;
static int fbclient_running = 0;
static int prober_running = 0;
static int android_device_connected = 0;

static int android_fbclient_thread(int arg)
{
	int socketfd, read_bytes;
	unsigned char* buffer;
	struct sockaddr_in address;
	int jpg_size;
	int remaining;
	queue input;
	DecodingInstance instance = NULL;
	int first = 1;
	int retry_times = 0;

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

	while(fbclient_running)
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
			if(!read_bytes)
				++retry_times;
			else
				retry_times = 0;
			if(retry_times > MAX_RETRY_READ_SOCKET)
				break;

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
	vpu_close_decoding_instance(&instance);
	free(buffer);
	queue_delete(&input);
	close(socketfd);
	fbclient_pthread = 0;
	fbclient_running = 0;
	return 0;
}

void android_fbclient_start(void)
{
	if(fbclient_running) return;
	fbclient_running = 1;
	pthread_create(&fbclient_pthread, NULL, (void*)android_fbclient_thread, NULL);
}

void android_fbclient_stop(void)
{
	int ret;
	if(!fbclient_running) return;
	fbclient_running = 0;
	pthread_join(fbclient_pthread, (void**)&ret);
	fbclient_pthread = 0;
}

int android_fbclient_is_running(void)
{
	return fbclient_running;
}


static int android_if_match_serial(const char* serial)
{
	FILE* fp;
	char read_serial[16] = {};

	fp = fopen(ANDROID_SERIAL_FILE, "r");

	while(!feof(fp))
	{
		fgets(read_serial, 9, fp);
		if(!strcmp(serial, read_serial))
			return 1;
	}

	return 0;
}

static int android_prober_thread(int arg)
{
	struct udev* udev = NULL;
	struct udev_monitor* udev_monitor = NULL;
	struct udev_device* dev = NULL;
	int udev_fd = -1;
	fd_set fds;
	struct timeval tv;
	int ret;
	char node_string[64] = {};
	char serial_string[10] = {};
	char serial_tmp[10] = {};


	prober_running = 1;

	udev = udev_new();
	udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "usb", NULL);
	udev_monitor_enable_receiving(udev_monitor);
	udev_fd = udev_monitor_get_fd(udev_monitor);

	while(prober_running)
	{
		FD_ZERO(&fds);
		FD_SET(udev_fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		ret = select(udev_fd + 1, &fds, NULL, NULL, &tv);

		if(ret > 0 && FD_ISSET(udev_fd, &fds))
		{
			dev = udev_monitor_receive_device(udev_monitor);

			if(dev&&!strcmp(udev_device_get_devtype(dev), "usb_device"))
			{
				if(!strcmp(udev_device_get_action(dev), "add"))
				{
					serial_tmp[0] = 0;
					strcat(serial_tmp, udev_device_get_sysattr_value(dev,"idVendor"));
					strcat(serial_tmp, udev_device_get_sysattr_value(dev,"idProduct"));

					fprintf(stderr, "USB Device [%s] added\n", serial_tmp);

					if(android_if_match_serial(serial_tmp) && !android_device_connected)
					{
						strncpy(serial_string, serial_tmp, 10);
						strncpy(node_string, udev_device_get_devnode(dev), 63);
						android_device_connected = 1;
						fputs("Device is supported by libmxdvr!\n", stderr);
					}

				}else if(!strcmp(udev_device_get_action(dev), "remove"))
				{
					if(strlen(node_string) > 0 && !strcmp(udev_device_get_devnode(dev), node_string))
					{
						fprintf(stderr, "USB Device [%s] removed\n", serial_string);
						node_string[0] = 0;
						android_device_connected = 0;
					}
				}

				udev_device_unref(dev);
			}
		}

		usleep(1000 * ANDROID_PROBE_INTERVAL);
	}

	udev_monitor_unref(udev_monitor);
	udev_unref(udev);
	udev = NULL;
	udev_monitor = NULL;
	return 0;
}

void android_prober_start(void)
{
	if(prober_running) return;
	pthread_create(&fbclient_pthread, NULL, (void*)android_prober_thread, NULL);
}

void android_prober_stop(void)
{
	int ret;
	if(!prober_running) return;
	prober_running = 0;
	pthread_join(fbclient_pthread, (void**)&ret);
}

int android_is_device_connected(void)
{
	return android_device_connected;
}

void android_fbserver_install(void)
{
	system("/root/adb kill-server");
	android_fbclient_stop();
	system("/root/adb push /root/Dserver /data/local/tmp/Dserver");
}

void android_fbserver_start(void)
{
	system("/root/adb kill-server");
	system("/root/adb forward tcp:5555 tcp:12500");
	android_fbserver_stop();
	system("/root/adb shell /data/local/tmp/Dserver &");
}

void android_fbserver_stop(void)
{
	FILE* fp = NULL;
	char pid[10] = {};
	char cmd[256] = {};
	fp = popen("./adb shell ps|grep Dserver|awk '{ print $2 }'", "r");
	fgets(pid, 9, fp);
	fclose(fp);
	pid[strlen(pid) - 1] = 0;

	strncat(cmd, "/root/adb shell kill -9 ", 255);
	strncat(cmd, pid, 255);
	system(cmd);
}
