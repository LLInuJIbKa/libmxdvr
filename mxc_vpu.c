#include <stdio.h>
#include "vpu_lib.h"
#include "mxc_vpu.h"
#include "framebuf.h"


int vpu_init(void)
{
	vpu_versioninfo ver;

	framebuf_init();
	vpu_Init(NULL);
	vpu_GetVersionInfo(&ver);

	fprintf(stderr, "VPU firmware version: %d.%d.%d\n", ver.fw_major, ver.fw_minor, ver.fw_release);
	fprintf(stderr, "VPU library version: %d.%d.%d\n", ver.lib_major, ver.lib_minor, ver.lib_release);
	return 0;
}

void vpu_uninit(void)
{
	vpu_UnInit();
}

