/**
 * @file framebuf.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 * @brief framebuf API from mxc_vpu_test
 * @details This module is exported from Freescale's sample program and only used in mxc_vpu module(mxc_vpu.h). The original source file is called <b>fb.c</b>.
 */
#ifndef FRAMEBUF_H_
#define FRAMEBUF_H_

#include "vpu_io.h"

struct frame_buf
{
	int addrY;
	int addrCb;
	int addrCr;
	int strideY;
	int strideC;
	int mvColBuf;
	vpu_mem_desc desc;
};

enum
{
	MODE420 = 0,
	MODE422 = 1,
	MODE224 = 2,
	MODE444 = 3,
	MODE400 = 4
};

void framebuf_init(void);
struct frame_buf *get_framebuf(void);
void put_framebuf(struct frame_buf *fb);
struct frame_buf *framebuf_alloc(int stdMode, int format, int strideY, int height);
void framebuf_free(struct frame_buf *fb);

#endif /* FRAMEBUF_H_ */
