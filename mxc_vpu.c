#include <stdio.h>
#include <stdlib.h>
#include "vpu_lib.h"
#include "mxc_vpu.h"

int vpu_init(void)
{
	vpu_versioninfo ver;

	vpu_Init(NULL);
	vpu_GetVersionInfo(&ver);

	fprintf(stderr, "VPU firmware version: %d.%d.%d\n", ver.fw_major, ver.fw_minor, ver.fw_release);
	fprintf(stderr, "VPU library version: %d.%d.%d\n", ver.lib_major, ver.lib_minor, ver.lib_release);


	return 0;
}
