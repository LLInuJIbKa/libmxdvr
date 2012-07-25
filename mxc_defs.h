#ifndef MXC_DEFS_H_
#define MXC_DEFS_H_

#include "v4l2dev.h"
#include "vpu_io.h"
#include "vpu_lib.h"
#include "framebuf.h"
#include "queue.h"
#include "platform.h"
#include <stdint.h>
#include <pthread.h>

#define STREAM_BUF_SIZE		(0x200000)
#define PS_SAVE_SIZE		(0x080000)

#ifndef u32
typedef Uint32 u32;
#endif
typedef unsigned short u16;
typedef unsigned char u8;

/**
 * @brief Instance object for decoding
 * @details You should <b>NOT</b> access this data structure directly.
 */
struct DecodingInstance
{
	DecHandle handle;
	PhysicalAddress phy_bsbuf_addr;
	PhysicalAddress phy_ps_buf;
	PhysicalAddress phy_slice_buf;
	int phy_slicebuf_size;
	u32 virt_bsbuf_addr;
	int picwidth;
	int picheight;
	int stride;
	int mjpg_fmt;
	int fbcount;
	int minFrameBufferCount;
	int rot_buf_count;
	FrameBuffer *fb;
	struct frame_buf **pfbpool;
	struct vpu_display *disp;
	vpu_mem_desc *mvcol_memdesc;
	Rect picCropRect;

	DecReportInfo frameBufStat;
	DecReportInfo userData;

	vpu_mem_desc mem_desc;

	int fps;
	DecParam decparam;
	int output_buffer_size;

	pthread_t thread;
	queue input_queue;
	queue output_queue;
	int run_thread;
	int show_timestamp;
};

/**
 * @brief Instance object for encoding
 * @details You should <b>NOT</b> access this data structure directly.
 */
struct EncodingInstance
{
	/** @brief Encoder handle */
	EncHandle handle;

	/** @brief Physical bitstream buffer */
	PhysicalAddress phy_bsbuf_addr;

	/** @brief Virtual bitstream buffer */
	u32 virt_bsbuf_addr;

	/** @brief Encoded Picture width */
	int enc_picwidth;

	/** @brief Encoded Picture height */
	int enc_picheight;

	/** @brief Source Picture width */
	int src_picwidth;

	/** @brief Source Picture height */
	int src_picheight;

	/** @brief Total number of framebuffers allocated */
	int fbcount;

	/** @brief Index of frame buffer that contains YUV image */
	int src_fbid;

	/** @brief Frame buffer base given to encoder */
	FrameBuffer *fb;

	/** @brief Allocated fb pointers are stored here */
	struct frame_buf **pfbpool;

	vpu_mem_desc mem_desc;
	int fd;
	EncParam enc_param;
	int input_size;

	pthread_t thread;
	queue input_queue;
	int run_thread;

};

extern pthread_mutex_t vpu_mutex;

#endif /* MXC_DEFS_H_ */
