#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/videodev2.h>
#include "vpu_io.h"
#include "mxc_vpu.h"
#include "framebuf.h"


#define STREAM_BUF_SIZE		(0x200000)
#define PS_SAVE_SIZE		(0x080000)
#define STREAM_END_SIZE		(0)
#define SIZE_USER_BUF		(0x1000)
#define STREAM_FILL_SIZE	(0x40000)

typedef unsigned long u32;
typedef int (*DATA_FILLER_FUNCTION)(DecodingInstance instance, unsigned char* buffer, const size_t size);


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


	InputType type;
	int format;
	int fps;
	DecParam decparam;


	int fd;

	unsigned char* buffer;
	int buffer_size;
	int buffer_read_position;


	DATA_FILLER_FUNCTION data_filler;
};


//static int vpu_read(const int fd, char *vptr, int n)
//{
//	int nleft = 0;
//	int nread = 0;
//	char *ptr;
//
//	ptr = vptr;
//	nleft = n;
//	while(nleft > 0)
//	{
//		if((nread = read(fd, ptr, nleft)) <= 0)
//		{
//			if(nread == 0)
//				return (n - nleft);
//
//			perror("read");
//			return (-1);
//		}
//
//		nleft -= nread;
//		ptr += nread;
//	}
//
//	return (nread);
//}


int data_fill_from_file(DecodingInstance instance, unsigned char* buffer, const size_t size)
{
	int data_read = 0;
	int left;
	unsigned char* ptr = buffer;


	left = size;
	while(left > 0)
	{
		data_read = read(instance->fd, buffer, left);
		if(data_read == 0) return (size - left);
		left -= data_read;
		ptr += data_read;
	}

	return data_read;
}


int data_fill_from_memory(DecodingInstance instance, unsigned char* buffer, const size_t size)
{
	int left = instance->buffer_size - instance->buffer_read_position;
	unsigned char* ptr = instance->buffer + instance->buffer_read_position;


	if(left)
	{
		memcpy(buffer, ptr, size);

		if(size <= left)
		{
			instance->buffer_read_position += size;
			fprintf(stderr, "Data read: %d bytes\n", size);
			return size;
		}
		else
		{
			fprintf(stderr, "Data read: %d bytes\n", left);
			return left;
		}
	}

	fprintf(stderr, "EOF\n", size);
	return -EAGAIN;

}





static int decoder_open(struct DecodingInstance* instance)
{
	RetCode ret;
	DecHandle handle = NULL;
	DecOpenParam oparam = {};

	oparam.bitstreamFormat = instance->format;
	oparam.bitstreamBuffer = instance->phy_bsbuf_addr;
	oparam.bitstreamBufferSize = STREAM_BUF_SIZE;
	oparam.reorderEnable = instance->reorderEnable;
//	oparam.mp4DeblkEnable = instance->cmdl->deblock_en;
//	oparam.chromaInterleave = instance->cmdl->chromaInterleave;
//	oparam.mp4Class = instance->cmdl->mp4Class;
	oparam.mjpg_thumbNailDecEnable = 0;

	/*
	 * mp4 deblocking filtering is optional out-loop filtering for image
	 * quality. In other words, mpeg4 deblocking is post processing.
	 * So, host application need to allocate new frame buffer.
	 * - On MX27, VPU doesn't support mpeg4 out loop deblocking filtering.
	 * - On MX37 and MX51, VPU control the buffer internally and return one
	 *   more buffer after vpu_DecGetInitialInfo().
	 * - On MX32, host application need to set frame buffer externally via
	 *   the command DEC_SET_DEBLOCK_OUTPUT.
	 */
	if(oparam.mp4DeblkEnable == 1)
	{
//		dec->cmdl->deblock_en = 0;
	}

	oparam.psSaveBuffer = instance->phy_ps_buf;
	oparam.psSaveBufferSize = PS_SAVE_SIZE;

	ret = vpu_DecOpen(&handle, &oparam);
	if (ret != RETCODE_SUCCESS) {
		return -1;
	}

	instance->handle = handle;
	return 0;
}

static int dec_fill_bsbuffer(struct DecodingInstance* instance, int defaultsize, int *eos, int *fill_end_bs)
{
	DecHandle handle = instance->handle;
	u32 bs_va_startaddr = instance->virt_bsbuf_addr;
	u32 bs_va_endaddr = (instance->virt_bsbuf_addr + STREAM_BUF_SIZE);
	u32 bs_pa_startaddr = instance->phy_bsbuf_addr;
	RetCode ret;
	PhysicalAddress pa_read_ptr, pa_write_ptr;
	u32 target_addr, space;
	int size;
	int nread, room;
	*eos = 0;

	ret = vpu_DecGetBitstreamBuffer(handle, &pa_read_ptr, &pa_write_ptr, &space);

	if(ret != RETCODE_SUCCESS)
	{
		return -1;
	}

	/* Decoder bitstream buffer is empty */
	if(space <= 0)
		return 0;

	if(defaultsize > 0)
	{
		if(space < defaultsize)
			return 0;

		size = defaultsize;
	}
	else
	{
		size = ((space >> 9) << 9);
	}

	if(size == 0)
		return 0;

	/* Fill the bitstream buffer */
	target_addr = bs_va_startaddr + (pa_write_ptr - bs_pa_startaddr);
	if((target_addr + size) > bs_va_endaddr)
	{
		room = bs_va_endaddr - target_addr;

		//if(instance->fd != -1)
		//	nread = vpu_read(instance->fd, (void*)target_addr, room);
		nread = instance->data_filler(instance, (void*)target_addr, room);


		if(nread <= 0)
		{
			/* EOF or error */
			if(nread < 0)
			{
				if(nread == -EAGAIN)
					return 0;

				return -1;
			}

			*eos = 1;
		}
		else
		{
			/* unable to fill the requested size, so back off! */
			if(nread != room)
				goto update;

			/* read the remaining */
			space = nread;
			//nread = vpu_read(instance->fd, (void*)bs_va_startaddr, (size - room));
			nread = instance->data_filler(instance, (void*)bs_va_startaddr, (size - room));

			if(nread <= 0)
			{
				/* EOF or error */
				if(nread < 0)
				{
					if(nread == -EAGAIN)
						return 0;

					return -1;
				}

				*eos = 1;
			}

			nread += space;
		}
	}
	else
	{
		//nread = vpu_read(instance->fd, (void*)target_addr, size);
		nread = instance->data_filler(instance, (void*)target_addr, size);
		if(nread <= 0)
		{
			/* EOF or error */
			if(nread < 0)
			{
				if(nread == -EAGAIN)
					return 0;

				return -1;
			}

			*eos = 1;
		}
	}

update:
	if(*eos == 0)
	{
		ret = vpu_DecUpdateBitstreamBuffer(handle, nread);
		if(ret != RETCODE_SUCCESS)
		{
			fprintf(stderr, "Fuck!\n");
			return -1;
		}
		*fill_end_bs = 0;
	}
	else
	{
		if(!*fill_end_bs)
		{
			ret = vpu_DecUpdateBitstreamBuffer(handle, STREAM_END_SIZE);
			if(ret != RETCODE_SUCCESS)
			{
				fprintf(stderr, "Fuck!\n");
				return -1;
			}
			*fill_end_bs = 1;
		}

	}

	return nread;
}

static int decoder_parse(struct DecodingInstance* instance)
{
	DecInitialInfo initinfo = {};
	DecHandle handle = instance->handle;
	int align, profile, level, extended_fbcount;
	RetCode ret;
	char *count;

	/*
	 * If userData report is enabled, buffer and comamnd need to be set
	 * before DecGetInitialInfo for MJPG.
	 */
	if(instance->userData.enable)
	{
		instance->userData.size = SIZE_USER_BUF;
		instance->userData.addr = malloc(SIZE_USER_BUF);
	}

	if(instance->format == STD_MJPG)
	{
		ret = vpu_DecGiveCommand(handle, DEC_SET_REPORT_USERDATA, &(instance->userData));
		if(ret != RETCODE_SUCCESS)
		{
			return -1;
		}
	}

	/* Parse bitstream and get width/height/framerate etc */
	vpu_DecSetEscSeqInit(handle, 1);
	ret = vpu_DecGetInitialInfo(handle, &initinfo);
	vpu_DecSetEscSeqInit(handle, 0);
	if(ret != RETCODE_SUCCESS)
	{
		return -1;
	}

	if(initinfo.streamInfoObtained)
	{
		switch(instance->format)
		{
		case STD_AVC:

			if(initinfo.aspectRateInfo)
			{
				int aspect_ratio_idc;
				int sar_width, sar_height;

				if((initinfo.aspectRateInfo >> 16) == 0)
				{
					aspect_ratio_idc = (initinfo.aspectRateInfo & 0xFF);
				}
				else
				{
					sar_width = (initinfo.aspectRateInfo >> 16);
					sar_height = (initinfo.aspectRateInfo & 0xFFFF);
				}
			}

			break;
		case STD_MJPG:
			instance->mjpg_fmt = initinfo.mjpg_sourceFormat;
			break;

		default:
			break;
		}
	}


#ifdef COMBINED_VIDEO_SUPPORT
	/* Following lines are sample code to support minFrameBuffer counter
	 changed in combined video stream. */
	if (dec->cmdl->format == STD_AVC)
	initinfo.minFrameBufferCount = 19;
#endif
	/*
	 * We suggest to add two more buffers than minFrameBufferCount:
	 *
	 * vpu_DecClrDispFlag is used to control framebuffer whether can be
	 * used for decoder again. One framebuffer dequeue from IPU is delayed
	 * for performance improvement and one framebuffer is delayed for
	 * display flag clear.
	 *
	 * Performance is better when more buffers are used if IPU performance
	 * is bottleneck.
	 *
	 * Two more buffers may be needed for interlace stream from IPU DVI view
	 */
	instance->minFrameBufferCount = initinfo.minFrameBufferCount;
	count = getenv("VPU_EXTENDED_BUFFER_COUNT");
	if(count)
		extended_fbcount = atoi(count);
	else
		extended_fbcount = 2;

	if(initinfo.interlace)
		instance->fbcount = initinfo.minFrameBufferCount + extended_fbcount + 2;
	else
		instance->fbcount = initinfo.minFrameBufferCount + extended_fbcount;

	instance->picwidth = ((initinfo.picWidth + 15) & ~15);

	align = 16;
	if((instance->format == STD_MPEG2 || instance->format == STD_VC1 || instance->format == STD_AVC) && initinfo.interlace == 1)
		align = 32;

	instance->picheight = ((initinfo.picHeight + align - 1) & ~(align - 1));

#ifdef COMBINED_VIDEO_SUPPORT
	/* Following lines are sample code to support resolution change
	 from small to large in combined video stream. MAX_FRAME_WIDTH
	 and MAX_FRAME_HEIGHT must be defined per user requirement. */
	if (dec->picwidth < MAX_FRAME_WIDTH)
	dec->picwidth = MAX_FRAME_WIDTH;
	if (dec->picheight < MAX_FRAME_HEIGHT)
	dec->picheight = MAX_FRAME_HEIGHT;
#endif

	fprintf(stderr, "W:%d, H:%d\n", instance->picwidth, instance->picheight);
	if((instance->picwidth == 0) || (instance->picheight == 0))
		return -1;

	/*
	 * Information about H.264 decoder picture cropping rectangle which
	 * presents the offset of top-left point and bottom-right point from
	 * the origin of frame buffer.
	 *
	 * By using these four offset values, host application can easily
	 * detect the position of target output window. When display cropping
	 * is off, the cropping window size will be 0.
	 *
	 * This structure for cropping rectangles is only valid for H.264
	 * decoder case.
	 */

	/* Add non-h264 crop support, assume left=top=0 */
	if((instance->picwidth > initinfo.picWidth || instance->picheight > initinfo.picHeight)
			&& (!initinfo.picCropRect.left && !initinfo.picCropRect.top && !initinfo.picCropRect.right
					&& !initinfo.picCropRect.bottom))
	{
		initinfo.picCropRect.left = 0;
		initinfo.picCropRect.top = 0;
		initinfo.picCropRect.right = initinfo.picWidth;
		initinfo.picCropRect.bottom = initinfo.picHeight;
	}

	memcpy(&(instance->picCropRect), &(initinfo.picCropRect), sizeof(initinfo.picCropRect));

	/* worstSliceSize is in kilo-byte unit */
	instance->phy_slicebuf_size = initinfo.worstSliceSize * 1024;
	instance->stride = instance->picwidth;

	/* Allocate memory for frame status, Mb and Mv report */
	if(instance->frameBufStat.enable)
	{
		instance->frameBufStat.addr = malloc(initinfo.reportBufSize.frameBufStatBufSize);
	}
	if(instance->mbInfo.enable)
	{
		instance->mbInfo.addr = malloc(initinfo.reportBufSize.mbInfoBufSize);
	}
	if(instance->mvInfo.enable)
	{
		instance->mvInfo.addr = malloc(initinfo.reportBufSize.mvInfoBufSize);
	}

	//info_msg("Display fps will be %d\n", instance->cmdl->fps);

	return 0;
}

static int decoder_allocate_framebuffer(struct DecodingInstance *instance)
{
	DecBufInfo bufinfo;
	int i, fbcount = instance->fbcount, totalfb, img_size;
//	int dst_scheme = instance->cmdl->dst_scheme, rot_en = instance->cmdl->rot_en;
//	int deblock_en = instance->cmdl->deblock_en;
//	int dering_en = instance->cmdl->dering_en;
//	struct rot rotation = {};
	RetCode ret;
	DecHandle handle = instance->handle;
	FrameBuffer *fb;
	struct frame_buf **pfbpool;
	struct vpu_display *disp = NULL;
	int stride, divX, divY;
	vpu_mem_desc *mvcol_md = NULL;
	Rect rotCrop;

//	rot_en = 0;

	/*
	 * 1 extra fb for deblocking on MX32, no need extrafb for blocking on MX37 and MX51
	 * dec->cmdl->deblock_en has been cleared to zero after set it to oparam.mp4DeblkEnable
	 * in decoder_open() function on MX37 and MX51.
	 */
//	if(deblock_en)
//	{
//		instance->extrafb++;
//	}

	totalfb = fbcount + instance->extrafb;

	fb = instance->fb = calloc(totalfb, sizeof(FrameBuffer));

	if(fb == NULL)
	{
		return -1;
	}

	pfbpool = instance->pfbpool = calloc(totalfb, sizeof(struct frame_buf *));
	if(pfbpool == NULL)
	{
		free(instance->fb);

		return -1;
	}

	//if(((dst_scheme != PATH_V4L2) && (dst_scheme != PATH_IPU)) || (((dst_scheme == PATH_V4L2) || (dst_scheme == PATH_IPU)) && deblock_en))
	{

		for(i = 0; i < totalfb; i++)
		{
			pfbpool[i] = framebuf_alloc(instance->format, instance->mjpg_fmt, instance->stride, instance->picheight);
			if(pfbpool[i] == NULL)
			{
				totalfb = i;
				goto err;
			}

			fb[i].bufY = pfbpool[i]->addrY;
			fb[i].bufCb = pfbpool[i]->addrCb;
			fb[i].bufCr = pfbpool[i]->addrCr;
			if(cpu_is_mx37() || cpu_is_mx5x())
			{
				fb[i].bufMvCol = pfbpool[i]->mvColBuf;
			}
		}
	}

//	if((dst_scheme == PATH_V4L2) || (dst_scheme == PATH_IPU))
	{
//		rotation.rot_en = 0;
//		rotation.rot_angle = 0;

//		if(dst_scheme == PATH_V4L2)
//			disp = v4l_display_open(instance, totalfb, rotation, instance->picCropRect);
//		else
//			disp = ipu_display_open(instance, totalfb, rotation, instance->picCropRect);

		if(disp == NULL)
		{
			goto err;
		}

		divX = (instance->mjpg_fmt == MODE420 || instance->mjpg_fmt == MODE422) ? 2 : 1;
		divY = (instance->mjpg_fmt == MODE420 || instance->mjpg_fmt == MODE224) ? 2 : 1;

//		if(deblock_en == 0)
		{
			img_size = instance->stride * instance->picheight;

			mvcol_md = instance->mvcol_memdesc = calloc(totalfb, sizeof(vpu_mem_desc));

			for(i = 0; i < totalfb; i++)
			{
//				if(dst_scheme == PATH_V4L2)
//					fb[i].bufY = disp->buffers[i]->offset;
//				else
//					fb[i].bufY = disp->ipu_bufs[i].ipu_paddr;
				fb[i].bufCb = fb[i].bufY + img_size;
				fb[i].bufCr = fb[i].bufCb + (img_size / divX / divY);
				/* allocate MvCol buffer here */
				memset(&mvcol_md[i], 0, sizeof(vpu_mem_desc));
				mvcol_md[i].size = img_size / divX / divY;
				ret = IOGetPhyMem(&mvcol_md[i]);
				if(ret)
				{
					goto err1;
				}
				fb[i].bufMvCol = mvcol_md[i].phy_addr;

			}
		}
	}

	stride = ((instance->stride + 15) & ~15);
	bufinfo.avcSliceBufInfo.sliceSaveBuffer = instance->phy_slice_buf;
	bufinfo.avcSliceBufInfo.sliceSaveBufferSize = instance->phy_slicebuf_size;

	/* User needs to fill max suported macro block value of frame as following*/
	bufinfo.maxDecFrmInfo.maxMbX = instance->stride / 16;
	bufinfo.maxDecFrmInfo.maxMbY = instance->picheight / 16;
	bufinfo.maxDecFrmInfo.maxMbNum = instance->stride * instance->picheight / 256;
	ret = vpu_DecRegisterFrameBuffer(handle, fb, fbcount, stride, &bufinfo);
	if(ret != RETCODE_SUCCESS)
	{
		goto err1;
	}

	instance->disp = disp;
	return 0;

	err1:
//	if(dst_scheme == PATH_V4L2)
//	{
//		v4l_display_close(disp);
//		instance->disp = NULL;
//	}
//	else if(dst_scheme == PATH_IPU)
//	{
//		ipu_display_close(disp);
//		instance->disp = NULL;
//	}

	err:

//	if(((dst_scheme != PATH_V4L2) && (dst_scheme != PATH_IPU)) || ((dst_scheme == PATH_V4L2) && deblock_en))
//	{
//		for(i = 0; i < totalfb; i++)
//		{
//			framebuf_free(pfbpool[i]);
//		}
//	}
//
//	if(fpFrmStatusLogfile)
//	{
//		fclose(fpFrmStatusLogfile);
//		fpFrmStatusLogfile = NULL;
//	}
//	if(fpErrMapLogfile)
//	{
//		fclose(fpErrMapLogfile);
//		fpErrMapLogfile = NULL;
//	}
//	if(fpQpLogfile)
//	{
//		fclose(fpQpLogfile);
//		fpQpLogfile = NULL;
//	}
//	if(fpSliceBndLogfile)
//	{
//		fclose(fpSliceBndLogfile);
//		fpSliceBndLogfile = NULL;
//	}
//	if(fpMvLogfile)
//	{
//		fclose(fpMvLogfile);
//		fpMvLogfile = NULL;
//	}
//	if(fpUserDataLogfile)
//	{
//		fclose(fpUserDataLogfile);
//		fpUserDataLogfile = NULL;
//	}

	free(instance->fb);
	free(instance->pfbpool);
	instance->fb = NULL;
	instance->pfbpool = NULL;
	return -1;
}


int vpu_decode_one_frame(DecodingInstance instance)
{
	DecHandle handle = instance->handle;
	DecOutputInfo outinfo =	{};
	int rot_en, rot_stride, fwidth, fheight;
//	int rot_angle = dec->cmdl->rot_angle;
//	int deblock_en = dec->cmdl->deblock_en;
//	int dering_en = dec->cmdl->dering_en;
	FrameBuffer *deblock_fb = NULL;
	FrameBuffer *fb = instance->fb;
	struct frame_buf **pfbpool = instance->pfbpool;
	struct frame_buf *pfb = NULL;
	struct vpu_display *disp = instance->disp;
	int err = 0, eos = 0, fill_end_bs = 0, decodefinish = 0;
	struct timeval tdec_begin, tdec_end, total_start, total_end;
	RetCode ret;
	int sec, usec, loop_id;
	u32 yuv_addr, img_size;
	double tdec_time = 0, frame_id = 0, total_time = 0;
	int decIndex = 0;
	int rotid = 0, dblkid = 0, mirror;
	int totalNumofErrMbs = 0;
	int disp_clr_index = -1, actual_display_index = -1, field = V4L2_FIELD_NONE;
	int is_waited_int = 0;
	char *delay_ms, *endptr;

	if(instance->type == MEMORYBLOCK && instance->buffer_size == 0) return -1;
	img_size = instance->picwidth * instance->picheight;
	ret = vpu_DecStartOneFrame(handle, &(instance->decparam));

	is_waited_int = 0;
	loop_id = 0;

	while(vpu_IsBusy())
	{
		err = dec_fill_bsbuffer(instance, STREAM_FILL_SIZE, &eos, &fill_end_bs);

		if(err < 0)
		{
			return -1;
		}

		if(loop_id == 10)
		{
			err = vpu_SWReset(handle, 0);
			return -1;
		}

		if(!err)
		{
			vpu_WaitForInt(500);
			is_waited_int = 1;
			loop_id++;
		}
	}

	if(!is_waited_int)
		vpu_WaitForInt(500);


	ret = vpu_DecGetOutputInfo(handle, &outinfo);

	if((instance->format == STD_MJPG) && (outinfo.indexFrameDisplay == 0))
	{
		outinfo.indexFrameDisplay = rotid;
	}

	fprintf(stderr, "frame_id = %d\n", (int)frame_id);
	if(ret != RETCODE_SUCCESS)
	{
		return -1;
	}

	if(outinfo.indexFrameDecoded >= 0)
	{
		if((instance->format == STD_AVC) || (instance->format == STD_MPEG4))
		{
			if((outinfo.interlacedFrame))
			{
				if(outinfo.topFieldFirst)
					field = V4L2_FIELD_INTERLACED_TB;
				else
					field = V4L2_FIELD_INTERLACED_BT;
				fprintf(stderr, "Top Field First flag: %d, dec_idx %d\n", outinfo.topFieldFirst, decIndex);
			}
		}
	}

	if(outinfo.indexFrameDecoded >= 0)
		decIndex++;

	if(outinfo.indexFrameDisplay == -1)
		decodefinish = 1;
	else if((outinfo.indexFrameDisplay > instance->fbcount) && (outinfo.prescanresult != 0))
		decodefinish = 1;


	actual_display_index = outinfo.indexFrameDisplay;


	if(err)
		return -1;

	if(outinfo.numOfErrMBs)
	{
		totalNumofErrMbs += outinfo.numOfErrMBs;
	}

	frame_id++;

	delay_ms = getenv("VPU_DECODER_DELAY_MS");
	if(delay_ms && strtol(delay_ms, &endptr, 10))
		usleep(strtol(delay_ms, &endptr, 10) * 1000);


	pfb = pfbpool[actual_display_index];


	yuv_addr = pfb->addrY + pfb->desc.virt_uaddr - pfb->desc.phy_addr;

	return yuv_addr;
}

DecodingInstance vpu_create_decoding_instance(void* input, const InputType type, const int format)
{
	DecodingInstance instance = NULL;
	int ret, eos = 0, fill_end_bs = 0, fillsize = 0;
	DecParam decparam = {};

	instance = calloc(1, sizeof(struct DecodingInstance));
	instance->mem_desc.size = STREAM_BUF_SIZE;
	ret = IOGetPhyMem(&(instance->mem_desc));

	IOGetVirtMem(&(instance->mem_desc));

	instance->phy_bsbuf_addr = instance->mem_desc.phy_addr;
	instance->virt_bsbuf_addr = instance->mem_desc.virt_uaddr;

	instance->reorderEnable = 1;

	instance->userData.enable = 0;
	instance->mbInfo.enable = 0;
	instance->mvInfo.enable = 0;
	instance->frameBufStat.enable = 0;

	if(format == STD_AVC)
	{
		instance->ps_mem_desc.size = PS_SAVE_SIZE;
		ret = IOGetPhyMem(&(instance->ps_mem_desc));
		instance->phy_ps_buf = instance->ps_mem_desc.phy_addr;
	}

	instance->format = format;
	instance->type = type;

	if(type == FILENAME)
	{
		instance->fd = open((const char*)input, O_RDONLY);
		instance->data_filler = data_fill_from_file;
	}else if(type == MEMORYBLOCK)
	{
		instance->buffer = (unsigned char*)input;
		instance->buffer_size = 100000;
		instance->data_filler = data_fill_from_memory;
	}


	ret = decoder_open(instance);
	ret = dec_fill_bsbuffer(instance, fillsize, &eos, &fill_end_bs);
	ret = decoder_parse(instance);

	if(format == STD_AVC)
	{
		instance->slice_mem_desc.size = instance->phy_slicebuf_size;
		ret = IOGetPhyMem(&(instance->slice_mem_desc));
		instance->phy_slice_buf = instance->slice_mem_desc.phy_addr;
	}

	ret = decoder_allocate_framebuffer(instance);


	decparam.dispReorderBuf = 0;
	decparam.prescanEnable = 0;
	decparam.prescanMode = 0;
	decparam.skipframeMode = 0;
	decparam.skipframeNum = 0;
	decparam.iframeSearchEnable = 0;


	return instance;
}

void vpu_set_input_buffer_size(DecodingInstance instance, const size_t size)
{
	if(instance->type != MEMORYBLOCK) return;
	instance->buffer_size = size;
}
