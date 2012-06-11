#ifndef MXC_DEFS_H_
#define MXC_DEFS_H_

#include "v4l2dev.h"
#include "vpu_io.h"
#include "vpu_lib.h"
#include "framebuf.h"
#include <stdint.h>

//#define STREAM_BUF_SIZE		(0x200000)
#define STREAM_BUF_SIZE		(0x200000)
#define PS_SAVE_SIZE		(0x080000)
#define STREAM_END_SIZE		(0)
#define SIZE_USER_BUF		(0x1000)
#define STREAM_FILL_SIZE	(0x40000)

#define MJPG_BUFFER_SIZE	(0x40000)

#ifndef u32
typedef Uint32 u32;
#endif
typedef unsigned short u16;
typedef unsigned char u8;

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
	int extrafb;
	FrameBuffer *fb;
	struct frame_buf **pfbpool;
	struct vpu_display *disp;
	vpu_mem_desc *mvcol_memdesc;
	Rect picCropRect;
	int reorderEnable;

	DecReportInfo mbInfo;
	DecReportInfo mvInfo;
	DecReportInfo frameBufStat;
	DecReportInfo userData;

	vpu_mem_desc mem_desc;
	vpu_mem_desc ps_mem_desc;
	vpu_mem_desc slice_mem_desc;


	v4l2dev device;
	int fps;
	DecParam decparam;


	int fd;

	unsigned char* buffer;
	int buffer_size;
	int buffer_read_position;

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

	EncReportInfo mbInfo;
	EncReportInfo mvInfo;
	EncReportInfo sliceInfo;

	vpu_mem_desc mem_desc;
	int fd;
	EncParam enc_param;
	int input_size;

};

#endif /* MXC_DEFS_H_ */
