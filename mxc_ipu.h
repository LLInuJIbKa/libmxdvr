/**
 * @file mxc_ipu.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 * @brief iMX5x IPU module
 * @details IPU on iMX5x boards provides some image process features like CSC(color space conversion), rotation, deinterlacing and display.<br><br>
 * Only <b>CSC</b> and <b>display</b> features are implemented in this module.<br><br>
 * Although mxc_ipu_hl_lib has a multi-instance design, seems like that it still can <b>NOT</b> run multiple tasks simultaneously.
 */
#ifndef MXC_IPU_H_
#define MXC_IPU_H_

#include <stdint.h>
#include <linux/ipu.h>
#include "mxc_ipu_hl_lib.h"

int ipu_query_task(void);

/**
 * @brief Initialize IPU.
 * @param in_w Width of the input image
 * @param in_h Height of the input image
 * @param in_fmt Pixel format of the input image
 * @param out_w Width of the output image
 * @param out_h Height of the output image
 * @param out_fmt Pixel format of the output image
 * @param show Render output image to framebuffer
 */
ipu_lib_handle_t* ipu_init(int in_w, int in_h, int in_fmt, int out_w, int out_h, int out_fmt, int show);

/**
 * @brief Uninitialize IPU.
 */
void ipu_uninit(ipu_lib_handle_t** ipu_handle);

/**
 * @brief Update IPU buffers.
 * @details IPU operations are done by calling this function. Please make sure that pointers are valid and allocated with enough memory.
 * @param input_data Pointer to the input image
 * @param output_data Pointer to the output image
 */
void ipu_buffer_update(ipu_lib_handle_t* ipu_handle, const unsigned char* input_data, unsigned char* output_data);

#endif /* MXC_IPU_H_ */
