/**
 * @file mxc_ipu.c
 * @author Ruei-Yuan Lu (ryuan_lu@iii.org.tw)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <memory.h>
#include "mxc_ipu_hl_lib.h"

#include "mxc_ipu.h"



static ipu_lib_handle_t* global_ipu_handle = NULL;
static int next_update_index = 0;



static void ipu_output_callback(void* output, int index)
{
	memcpy(output, global_ipu_handle->outbuf_start[index], global_ipu_handle->ofr_size);
}


int ipu_query_task(void)
{
	int i;
	ipu_lib_ctl_task_t task;

	for (i = 0; i< MAX_TASK_NUM; i++) {
		task.index = i;
		mxc_ipu_lib_task_control(IPU_CTL_TASK_QUERY, (void *)(&task), NULL);
		if (task.task_pid) {
			fprintf(stderr, "\ntask %d:\n", i);
			fprintf(stderr, "\tpid: %d\n", task.task_pid);
			fprintf(stderr, "\tmode:\n");
			if (task.task_mode & IC_ENC)
				fprintf(stderr, "\t\tIC_ENC\n");
			if (task.task_mode & IC_VF)
				fprintf(stderr, "\t\tIC_VF\n");
			if (task.task_mode & IC_PP)
				fprintf(stderr, "\t\tIC_PP\n");
			if (task.task_mode & ROT_ENC)
				fprintf(stderr, "\t\tROT_ENC\n");
			if (task.task_mode & ROT_VF)
				fprintf(stderr, "\t\tROT_VF\n");
			if (task.task_mode & ROT_PP)
				fprintf(stderr, "\t\tROT_PP\n");
			if (task.task_mode & VDI_IC_VF)
				fprintf(stderr, "\t\tVDI_IC_VF\n");
		}
	}

	return 0;
}

/**
 * @brief Initialize IPU
 * @param in_w Width of the input image
 * @param in_h Height of the input image
 * @param in_fmt Pixel format of the input image
 * @param out_w Width of the output image
 * @param out_h Height of the output image
 * @param out_fmt Pixel format of the output image
 * @param show Render output image to framebuffer
 */
int ipu_init(int in_w, int in_h, int in_fmt, int out_w, int out_h, int out_fmt, int show)
{
	ipu_lib_input_param_t input;
	ipu_lib_output_param_t output;

	if(global_ipu_handle)
		ipu_uninit();

	global_ipu_handle = calloc(1, sizeof(ipu_lib_handle_t));
	memset(&input, 0, sizeof(ipu_lib_input_param_t));
	memset(&output, 0, sizeof(ipu_lib_output_param_t));

	input.width = in_w;
	input.height = in_h;
	input.fmt = in_fmt;

	output.width = out_w;
	output.height = out_h;
	output.fmt = out_fmt;
	output.show_to_fb = show;

	return mxc_ipu_lib_task_init(&input, NULL, &output, OP_NORMAL_MODE, global_ipu_handle);
}

/**
 * @brief Uninitialize IPU
 */
void ipu_uninit(void)
{
	if(!global_ipu_handle)
		return;

	mxc_ipu_lib_task_uninit(global_ipu_handle);
	free(global_ipu_handle);
	global_ipu_handle = NULL;
}

/**
 * @brief Update IPU buffers.
 * @details IPU operations are done by calling this function. Please make sure that pointers are valid and allocated with enough memory.
 * @param input_data Pointer to the input image
 * @param output_data Pointer to the output image
 */
void ipu_buffer_update(const unsigned char* input_data, unsigned char* output_data)
{
	memcpy(global_ipu_handle->inbuf_start[next_update_index], input_data, global_ipu_handle->ifr_size);
	next_update_index = mxc_ipu_lib_task_buf_update(global_ipu_handle, 0, 0, 0, ipu_output_callback, output_data);
}

