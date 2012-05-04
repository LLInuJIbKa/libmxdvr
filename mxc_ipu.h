#ifndef MXC_IPU_H_
#define MXC_IPU_H_

#include <stdint.h>
#include <linux/ipu.h>
#include "mxc_ipu_hl_lib.h"

int ipu_query_task(void);
ipu_lib_handle_t* ipu_init(int in_w, int in_h, int in_fmt, int out_w, int out_h, int out_fmt, int show);
void ipu_uninit(ipu_lib_handle_t** ipu_handle);
void ipu_buffer_update(ipu_lib_handle_t* ipu_handle, const unsigned char* input_data, unsigned char* output_data);

#endif /* MXC_IPU_H_ */
