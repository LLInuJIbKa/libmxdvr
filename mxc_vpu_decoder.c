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
typedef unsigned short u16;
typedef unsigned char u8;
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


static int freadn(int fd, void *vptr, size_t n)
{
	int nleft = 0;
	int nread = 0;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while(nleft > 0)
	{
		if((nread = read(fd, ptr, nleft)) <= 0)
		{
			if(nread == 0)
				return (n - nleft);

			perror("read");
			return (-1);
		}

		nleft -= nread;
		ptr += nread;
	}

	return (nread);
}

static int vpu_read(int fd, char *buf, int n)
{
	return freadn(fd, buf, n);
}


static int dec_fill_bsbuffer(DecodingInstance dec,
		u32 bs_va_startaddr, u32 bs_va_endaddr,
		u32 bs_pa_startaddr, int defaultsize,
		int *eos, int *fill_end_bs)
{
	RetCode ret;
	DecHandle handle = dec->handle;
	PhysicalAddress pa_read_ptr, pa_write_ptr;
	u32 target_addr, space;
	int size;
	int nread, room;
	*eos = 0;

	ret = vpu_DecGetBitstreamBuffer(handle, &pa_read_ptr, &pa_write_ptr, &space);
	if(ret != RETCODE_SUCCESS)
	{
		fputs("vpu_DecGetBitstreamBuffer failed\n", stderr);
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
		nread = vpu_read(dec->fd, (void *)target_addr, room);
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
			nread = vpu_read(dec->fd, (void *)bs_va_startaddr, (size - room));
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
		nread = vpu_read(dec->fd, (void *)target_addr, size);
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

	update: if(*eos == 0)
	{
		ret = vpu_DecUpdateBitstreamBuffer(handle, nread);
		if(ret != RETCODE_SUCCESS)
		{
			fputs("vpu_DecUpdateBitstreamBuffer failed\n", stderr);
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
				fputs("vpu_DecUpdateBitstreamBuffer failed\n", stderr);
				return -1;
			}
			*fill_end_bs = 1;
		}

	}

	return nread;
}


static int decoder_open(DecodingInstance dec)
{
	RetCode ret;
	DecHandle handle = NULL;
	DecOpenParam oparam = {};

	oparam.bitstreamFormat = dec->format;
	oparam.bitstreamBuffer = dec->phy_bsbuf_addr;
	oparam.bitstreamBufferSize = STREAM_BUF_SIZE;
	oparam.reorderEnable = dec->reorderEnable;
	oparam.mp4DeblkEnable = 0;
	oparam.chromaInterleave = 0;
	oparam.mp4Class = 0;
	oparam.mjpg_thumbNailDecEnable = 0;
	oparam.psSaveBuffer = dec->phy_ps_buf;
	oparam.psSaveBufferSize = PS_SAVE_SIZE;

	ret = vpu_DecOpen(&handle, &oparam);
	if (ret != RETCODE_SUCCESS) {
		fputs("vpu_DecOpen failed\n", stderr);
		return -1;
	}

	dec->handle = handle;
	return 0;
}

static int decoder_parse(DecodingInstance dec)
{
	DecInitialInfo initinfo = {0};
	DecHandle handle = dec->handle;
	int align, profile, level, extended_fbcount;
	RetCode ret;
	char *count;


	if(dec->format == STD_MJPG)
	{
		ret = vpu_DecGiveCommand(handle, DEC_SET_REPORT_USERDATA, &dec->userData);
		if(ret != RETCODE_SUCCESS)
		{
			fprintf(stderr, "Failed to set user data report, ret %d\n", ret);
			return -1;
		}
	}

	vpu_DecSetEscSeqInit(handle, 1);
	ret = vpu_DecGetInitialInfo(handle, &initinfo);
	vpu_DecSetEscSeqInit(handle, 0);

	if (ret != RETCODE_SUCCESS)
	{
		fprintf(stderr, "vpu_DecGetInitialInfo failed, ret:%d, errorcode:%d\n", ret, initinfo.errorcode);
		return -1;
	}

	if(initinfo.streamInfoObtained)
	{
		if(dec->format == STD_MJPG)
			dec->mjpg_fmt = initinfo.mjpg_sourceFormat;
	}

	dec->minFrameBufferCount = initinfo.minFrameBufferCount;

	count = getenv("VPU_EXTENDED_BUFFER_COUNT");
	if(count)
		extended_fbcount = atoi(count);
	else
		extended_fbcount = 2;

	dec->fbcount = initinfo.minFrameBufferCount + extended_fbcount;

	dec->picwidth = ((initinfo.picWidth + 15) & ~15);
	align = 16;
	dec->picheight = ((initinfo.picHeight + align - 1) & ~(align - 1));

	if ((dec->picwidth == 0) || (dec->picheight == 0))
		return -1;

	if((dec->picwidth > initinfo.picWidth || dec->picheight > initinfo.picHeight)
			&& (!initinfo.picCropRect.left && !initinfo.picCropRect.top && !initinfo.picCropRect.right
					&& !initinfo.picCropRect.bottom))
	{
		initinfo.picCropRect.left = 0;
		initinfo.picCropRect.top = 0;
		initinfo.picCropRect.right = initinfo.picWidth;
		initinfo.picCropRect.bottom = initinfo.picHeight;
	}


	memcpy(&(dec->picCropRect), &(initinfo.picCropRect), sizeof(initinfo.picCropRect));

	dec->phy_slicebuf_size = initinfo.worstSliceSize * 1024;
	dec->stride = dec->picwidth;

	return 0;
}

static int decoder_allocate_framebuffer(DecodingInstance dec)
{
	DecBufInfo bufinfo;
	int i, fbcount = dec->fbcount, totalfb, img_size;
	RetCode ret;
	DecHandle handle = dec->handle;
	FrameBuffer *fb;
	struct frame_buf **pfbpool;
	struct vpu_display *disp = NULL;
	int stride, divX, divY;
	vpu_mem_desc *mvcol_md = NULL;
	Rect rotCrop;

	totalfb = fbcount + dec->extrafb;

	fb = dec->fb = calloc(totalfb, sizeof(FrameBuffer));

	pfbpool = dec->pfbpool = calloc(totalfb, sizeof(struct frame_buf *));


	for(i = 0; i < totalfb; i++)
	{
		pfbpool[i] = framebuf_alloc(dec->format, dec->mjpg_fmt, dec->stride, dec->picheight);
		if(pfbpool[i] == NULL)
		{
			totalfb = i;
			goto err;
		}

		fb[i].bufY = pfbpool[i]->addrY;
		fb[i].bufCb = pfbpool[i]->addrCb;
		fb[i].bufCr = pfbpool[i]->addrCr;
		fb[i].bufMvCol = pfbpool[i]->mvColBuf;

	}

	stride = ((dec->stride + 15) & ~15);
	bufinfo.avcSliceBufInfo.sliceSaveBuffer = dec->phy_slice_buf;
	bufinfo.avcSliceBufInfo.sliceSaveBufferSize = dec->phy_slicebuf_size;

	bufinfo.maxDecFrmInfo.maxMbX = dec->stride / 16;
	bufinfo.maxDecFrmInfo.maxMbY = dec->picheight / 16;
	bufinfo.maxDecFrmInfo.maxMbNum = dec->stride * dec->picheight / 256;
	ret = vpu_DecRegisterFrameBuffer(handle, fb, fbcount, stride, &bufinfo);

	if(ret != RETCODE_SUCCESS)
	{
		goto err1;
	}

	dec->disp = disp;
	return 0;

err1:

err:
	for(i = 0; i < totalfb; i++)
	{
		framebuf_free(pfbpool[i]);
	}

	free(dec->fb);
	free(dec->pfbpool);
	dec->fb = NULL;
	dec->pfbpool = NULL;
	return -1;
}

DecodingInstance vpu_create_decoding_instance(void* input, const InputType type, const int format)
{
	DecodingInstance dec = NULL;
	DecParam decparam = {};
	int rot_stride = 0, fwidth, fheight, rot_angle = 0, mirror = 0;
	int rotid = 0;
	int eos = 0, fill_end_bs = 0, fillsize = 0;


	dec = calloc(1, sizeof(struct DecodingInstance));

	dec->mem_desc.size = STREAM_BUF_SIZE;
	IOGetPhyMem(&dec->mem_desc);
	IOGetVirtMem(&dec->mem_desc);

	dec->phy_bsbuf_addr = dec->mem_desc.phy_addr;
	dec->virt_bsbuf_addr = dec->mem_desc.virt_uaddr;
	dec->reorderEnable = 1;
	dec->format = format;

	if(format == STD_AVC)
	{
		dec->ps_mem_desc.size = PS_SAVE_SIZE;
		IOGetPhyMem(&(dec->ps_mem_desc));
		dec->phy_ps_buf = dec->ps_mem_desc.phy_addr;
	}

	dec->type = type;
	if(type == FILENAME)
	{
		dec->fd = open((char*)input, O_RDONLY);
	}


	decoder_open(dec);
	dec_fill_bsbuffer(dec,
				dec->virt_bsbuf_addr,
			        (dec->virt_bsbuf_addr + STREAM_BUF_SIZE),
				dec->phy_bsbuf_addr, fillsize, &eos, &fill_end_bs);
	decoder_parse(dec);

	if(format == STD_AVC)
	{
		dec->slice_mem_desc.size = dec->phy_slicebuf_size;
		IOGetPhyMem(&(dec->slice_mem_desc));
		dec->phy_slice_buf = dec->slice_mem_desc.phy_addr;
	}

	decoder_allocate_framebuffer(dec);


	if(dec->format == STD_MJPG)
		rotid = 0;

	decparam.dispReorderBuf = 0;
	decparam.prescanEnable = 0;
	decparam.prescanMode = 0;
	decparam.skipframeMode = 0;
	decparam.skipframeNum = 0;
	decparam.iframeSearchEnable = 0;

	fwidth = ((dec->picwidth + 15) & ~15);
	fheight = ((dec->picheight + 15) & ~15);

	if(dec->format == STD_MJPG)
	{
		vpu_DecGiveCommand(dec->handle, SET_ROTATION_ANGLE, &rot_angle);
		vpu_DecGiveCommand(dec->handle, SET_MIRROR_DIRECTION, &mirror);
		rot_stride = fwidth;
		vpu_DecGiveCommand(dec->handle, SET_ROTATOR_STRIDE, &rot_stride);
	}

	//img_size = dec->picwidth * dec->picheight * 3 / 2;

	return dec;
}

int vpu_decode_one_frame(DecodingInstance dec, unsigned char* output)
{
	DecHandle handle = dec->handle;
	FrameBuffer *fb = dec->fb;
	DecOutputInfo outinfo = {};
	int rotid = 0;
	int ret;
	int is_waited_int;
	int loop_id;
	double frame_id = 0;
	int disp_clr_index = -1, actual_display_index = -1;
	struct frame_buf **pfbpool = dec->pfbpool;
	struct frame_buf *pfb = NULL;
	u32 yuv_addr, img_size = dec->picwidth * dec->picheight * 2;
	int decIndex = 0;
	int err = 0, eos = 0, fill_end_bs = 0, decodefinish = 0;

	if(dec->format == STD_MJPG)
	{
		vpu_DecGiveCommand(handle, SET_ROTATOR_OUTPUT, (void *)&fb[rotid]);
	}

	ret = vpu_DecStartOneFrame(handle, &(dec->decparam));

	if(ret != RETCODE_SUCCESS)
	{
		fputs("DecStartOneFrame failed\n", stderr);
		return -1;
	}

	is_waited_int = 0;
	loop_id = 0;

	while(vpu_IsBusy())
	{
		err = dec_fill_bsbuffer(dec,
			      dec->virt_bsbuf_addr,
			      (dec->virt_bsbuf_addr + STREAM_BUF_SIZE),
			      dec->phy_bsbuf_addr, STREAM_FILL_SIZE,
			      &eos, &fill_end_bs);

		if(err < 0)
		{
			fputs("dec_fill_bsbuffer failed\n", stderr);
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

	if((dec->format == STD_MJPG) && (outinfo.indexFrameDisplay == 0))
	{
		outinfo.indexFrameDisplay = rotid;
	}

	dprintf(4, "frame_id = %d\n", (int)frame_id);
	if(ret != RETCODE_SUCCESS)
	{
		fprintf(stderr, "vpu_DecGetOutputInfo failed Err code is %d\n\tframe_id = %d\n", ret, (int)frame_id);
		return -1;
	}

	if(outinfo.decodingSuccess == 0)
	{
		fprintf(stderr, "Incomplete finish of decoding process.\n\tframe_id = %d\n", (int)frame_id);
		return 0;
	}

	if(outinfo.notSufficientPsBuffer)
	{
		fputs("PS Buffer overflow\n", stderr);
		return -1;
	}

	if(outinfo.notSufficientSliceBuffer)
	{
		fputs("Slice Buffer overflow\n", stderr);
		return -1;
	}

	if(outinfo.indexFrameDecoded >= 0)
		decIndex++;

	if(outinfo.indexFrameDisplay == -1)
		decodefinish = 1;
	else if((outinfo.indexFrameDisplay > dec->fbcount) && (outinfo.prescanresult != 0))
		decodefinish = 1;

	if(decodefinish)
		return -1;

	if((outinfo.indexFrameDisplay == -3) || (outinfo.indexFrameDisplay == -2))
	{
		dprintf(3, "VPU doesn't have picture to be displayed.\n\toutinfo.indexFrameDisplay = %d\n", outinfo.indexFrameDisplay);

		if(dec->format != STD_MJPG && disp_clr_index >= 0)
		{
			err = vpu_DecClrDispFlag(handle, disp_clr_index);
			if(err)
				fprintf(stderr, "vpu_DecClrDispFlag failed Error code %d\n", err);
		}
		disp_clr_index = outinfo.indexFrameDisplay;

		return 0;
	}

	if(dec->format == STD_MJPG)
		actual_display_index = rotid;
	else
		actual_display_index = outinfo.indexFrameDisplay;


	pfb = pfbpool[actual_display_index];

	yuv_addr = pfb->addrY + pfb->desc.virt_uaddr - pfb->desc.phy_addr;

	//write_to_file(dec, (u8 *)yuv_addr, dec->picCropRect);
	memcpy(output, (unsigned char*)yuv_addr, img_size);

	if(dec->format != STD_MJPG && disp_clr_index >= 0)
	{
		err = vpu_DecClrDispFlag(handle, disp_clr_index);
		if(err)
			fprintf(stderr, "vpu_DecClrDispFlag failed Error code %d\n", err);
	}

	disp_clr_index = outinfo.indexFrameDisplay;

	frame_id++;
	return 0;
}

