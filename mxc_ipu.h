#ifndef MXC_IPU_H_
#define MXC_IPU_H_

#include <stdint.h>
#include <linux/ipu.h>

int ipu_query_task(void);
int ipu_init(int in_w, int in_h, int in_fmt, int out_w, int out_h, int out_fmt, int show);
void ipu_uninit(void);
void ipu_buffer_update(const unsigned char* input_data, unsigned char* output_data);

#endif /* MXC_IPU_H_ */
