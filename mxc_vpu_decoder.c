#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "mxc_defs.h"
#include "mxc_vpu.h"
#include "mxc_display.h"
#include "font.h"

#define VPU_DECODING_QUEUE_SIZE	(64)

static int fill_bsbuffer(DecodingInstance dec, int defaultsize, int *eos, int *fill_end_bs)
{
	RetCode ret = 0;
	DecHandle handle = dec->handle;
	PhysicalAddress pa_read_ptr, pa_write_ptr;
	u32 target_addr;
	int nread;
	u32 bs_va_startaddr = dec->virt_bsbuf_addr;
	u32 bs_va_endaddr = dec->virt_bsbuf_addr + STREAM_BUF_SIZE;
	u32 bs_pa_startaddr = dec->phy_bsbuf_addr;
	Uint32 space = 0;
	u32 room;
	unsigned char* ptr = NULL;
	*eos = 0;

	ret = vpu_DecGetBitstreamBuffer(handle, &pa_read_ptr, &pa_write_ptr, &space);
	if(ret != RETCODE_SUCCESS) return 0;

	target_addr = bs_va_startaddr + (pa_write_ptr - bs_pa_startaddr);


	/* Decoder bitstream buffer is empty */
	if(space <= 0)
		return 0;

	ptr = dec->input_buffer;
	nread = queue_get_buffer_size(dec->input_queue);

	if(nread>space)
	{
		return 0;
	}

	if((target_addr + nread) > bs_va_endaddr)
	{
		room = bs_va_endaddr - target_addr;
		memcpy((void*)target_addr, ptr, room);
		memcpy((void*)bs_va_startaddr, ptr+room, nread - room);

	}
	else
	{
		MEMCPY((void*)target_addr, ptr, nread);
	}

	vpu_DecUpdateBitstreamBuffer(handle, nread);

	return nread;
}

static int decoder_open(DecodingInstance dec)
{
	RetCode ret;
	DecHandle handle = NULL;
	DecOpenParam oparam = {};

	oparam.bitstreamFormat = STD_MJPG;
	oparam.bitstreamBuffer = dec->phy_bsbuf_addr;
	oparam.bitstreamBufferSize = STREAM_BUF_SIZE;
	oparam.reorderEnable = 1;
	oparam.mp4DeblkEnable = 0;
	oparam.chromaInterleave = 0;
	oparam.mp4Class = 0;
	oparam.mjpg_thumbNailDecEnable = 0;
	oparam.psSaveBuffer = dec->phy_ps_buf;
	oparam.psSaveBufferSize = PS_SAVE_SIZE;

	ret = vpu_DecOpen(&handle, &oparam);
	if(ret != RETCODE_SUCCESS)
	{
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
	int align, extended_fbcount;
	RetCode ret;
	char *count;


	ret = vpu_DecGiveCommand(handle, DEC_SET_REPORT_USERDATA, &dec->userData);
	if(ret != RETCODE_SUCCESS)
	{
		fprintf(stderr, "Failed to set user data report, ret %d\n", ret);
		return -1;
	}


	vpu_DecSetEscSeqInit(handle, 1);
	ret = vpu_DecGetInitialInfo(handle, &initinfo);
	vpu_DecSetEscSeqInit(handle, 0);

	if (ret != RETCODE_SUCCESS)
	{
		fprintf(stderr, "vpu_DecGetInitialInfo failed, ret:%d, errorcode:%lu\n", ret, initinfo.errorcode);
		return -1;
	}

	if(initinfo.streamInfoObtained)
	{
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

	totalfb = fbcount;

	fb = dec->fb = calloc(totalfb, sizeof(FrameBuffer));

	pfbpool = dec->pfbpool = calloc(totalfb, sizeof(struct frame_buf *));


	disp = v4l_display_open(dec, totalfb, 1024, 768, 0, 0);

	if(disp == NULL)
		goto err;

	divX = (dec->mjpg_fmt == MODE420 || dec->mjpg_fmt == MODE422) ? 2 : 1;
	divY = (dec->mjpg_fmt == MODE420 || dec->mjpg_fmt == MODE224) ? 2 : 1;

	img_size = dec->stride * dec->picheight;
	dec->output_buffer_size = img_size * (dec->mjpg_fmt == MODE422 ? 2.0 : 1.5);


	mvcol_md = dec->mvcol_memdesc = calloc(totalfb, sizeof(vpu_mem_desc));

	for(i = 0; i < totalfb; i++)
	{
		fb[i].bufY = disp->buffers[i]->offset;
		fb[i].bufCb = fb[i].bufY + img_size;
		fb[i].bufCr = fb[i].bufCb + (img_size / divX / divY);
		/* allocate MvCol buffer here */
		memset(&mvcol_md[i], 0, sizeof(vpu_mem_desc));
		mvcol_md[i].size = img_size / divX / divY;
		IOGetPhyMem(&mvcol_md[i]);
		fb[i].bufMvCol = mvcol_md[i].phy_addr;

	}

	stride = ((dec->stride + 15) & ~15);
	bufinfo.avcSliceBufInfo.sliceSaveBuffer = dec->phy_slice_buf;
	bufinfo.avcSliceBufInfo.sliceSaveBufferSize = dec->phy_slicebuf_size;

	bufinfo.maxDecFrmInfo.maxMbX = dec->stride / 16;
	bufinfo.maxDecFrmInfo.maxMbY = dec->picheight / 16;
	bufinfo.maxDecFrmInfo.maxMbNum = dec->stride * dec->picheight / 256;
	ret = vpu_DecRegisterFrameBuffer(handle, fb, fbcount, stride, &bufinfo);

	if(ret != RETCODE_SUCCESS)
		goto err1;

	dec->disp = disp;
	return 0;

err1:
	v4l_display_close(disp);
	dec->disp = NULL;
err:
	free(dec->fb);
	free(dec->pfbpool);
	dec->fb = NULL;
	dec->pfbpool = NULL;
	return -1;
}

DecodingInstance vpu_create_decoding_instance_for_v4l2(queue input)
{
	DecodingInstance dec = NULL;
	DecParam decparam = {};
	int rot_stride = 0, fwidth, fheight, rot_angle = 0, mirror = 0;
	int rotid = 0;
	int eos = 0, fill_end_bs = 0;


	dec = calloc(1, sizeof(struct DecodingInstance));

	dec->mem_desc.size = STREAM_BUF_SIZE;
	IOGetPhyMem(&dec->mem_desc);
	IOGetVirtMem(&dec->mem_desc);

	dec->phy_bsbuf_addr = dec->mem_desc.phy_addr;
	dec->virt_bsbuf_addr = dec->mem_desc.virt_uaddr;
	dec->input_queue = input;
	dec->input_buffer = calloc(1, queue_get_buffer_size(dec->input_queue));

	decoder_open(dec);

	while(queue_pop(dec->input_queue, dec->input_buffer) == -1)
		usleep(0);
	fill_bsbuffer(dec, 1, &eos, &fill_end_bs);

	decoder_parse(dec);

	decoder_allocate_framebuffer(dec);

	rotid = 0;

	decparam.dispReorderBuf = 0;
	decparam.prescanEnable = 0;
	decparam.prescanMode = 0;
	decparam.skipframeMode = 0;
	decparam.skipframeNum = 0;
	decparam.iframeSearchEnable = 0;

	fwidth = ((dec->picwidth + 15) & ~15);
	fheight = ((dec->picheight + 15) & ~15);
	rot_stride = fwidth;

	vpu_DecGiveCommand(dec->handle, SET_ROTATION_ANGLE, &rot_angle);
	vpu_DecGiveCommand(dec->handle, SET_MIRROR_DIRECTION, &mirror);
	vpu_DecGiveCommand(dec->handle, SET_ROTATOR_STRIDE, &rot_stride);

	return dec;
}

int vpu_decode_one_frame(DecodingInstance dec, unsigned char** output)
{
	DecHandle handle = dec->handle;
	FrameBuffer *fb = dec->fb;
	DecOutputInfo outinfo = {};
	static int rotid = 0;
	int ret;
	int is_waited_int;
	int loop_id;
	double frame_id = 0;
	int actual_display_index = -1;
	int decIndex = 0;
	int err = 0, eos = 0, fill_end_bs = 0, decodefinish = 0;
	struct vpu_display *disp = dec->disp;

	while(queue_pop(dec->input_queue, dec->input_buffer) == -1)
		usleep(0);

	vpu_DecGiveCommand(handle, SET_ROTATOR_OUTPUT, (void *)&fb[rotid]);
	pthread_mutex_lock(&vpu_mutex);
	ret = vpu_DecStartOneFrame(handle, &(dec->decparam));

	if(ret != RETCODE_SUCCESS)
	{
		fputs("DecStartOneFrame failed\n", stderr);
		return -1;
	}

	is_waited_int = 0;
	loop_id = 0;

	if(vpu_IsBusy())
	{
		err = fill_bsbuffer(dec, 0, &eos, &fill_end_bs);

		if(err < 0)
		{
			fputs("dec_fill_bsbuffer failed\n", stderr);
			return -1;
		}

		vpu_WaitForInt(500);
	}else
		return -1;

	ret = vpu_DecGetOutputInfo(handle, &outinfo);
	pthread_mutex_unlock(&vpu_mutex);

	if(outinfo.indexFrameDisplay == 0)
	{
		outinfo.indexFrameDisplay = rotid;
	}

	if(outinfo.decodingSuccess == 0)
	{
		fprintf(stderr, "Incomplete finish of decoding process.\n\tframe_id = %d\n", (int)frame_id);
		*output = NULL;
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

	actual_display_index = rotid;
	*output = disp->buffers[actual_display_index]->start;

	rotid++;
	rotid %= dec->fbcount;
	dec->rot_buf_count = rotid;
	frame_id++;
	return 0;
}

void vpu_close_decoding_instance(DecodingInstance* instance)
{
	DecodingInstance handle = *instance;
	DecOutputInfo outinfo = {};
	RetCode ret;
	int i;
	vpu_mem_desc *mvcol_md = handle->mvcol_memdesc;


	ret = vpu_DecClose(handle->handle);
	if(ret == RETCODE_FRAME_NOT_COMPLETE)
	{
		vpu_DecGetOutputInfo(handle->handle, &outinfo);
		ret = vpu_DecClose(handle->handle);
	}

	if(handle->disp)
	{
		v4l_display_close(handle->disp);
		handle->disp = NULL;
	}

	if(mvcol_md)
	{
		for(i = 0; i < handle->fbcount; i++)
		{
			if(mvcol_md[i].phy_addr)
				IOFreePhyMem(&mvcol_md[i]);
		}
		if(handle->mvcol_memdesc)
		{
			free(handle->mvcol_memdesc);
			handle->mvcol_memdesc = NULL;
		}
	}

	if(handle->fb)
	{
		free(handle->fb);
		handle->fb = NULL;
	}
	if(handle->pfbpool)
	{
		free(handle->pfbpool);
		handle->pfbpool = NULL;
	}

	if(handle->frameBufStat.addr)
		free(handle->frameBufStat.addr);

	if(handle->userData.addr)
		free(handle->userData.addr);

	IOFreeVirtMem(&(handle->mem_desc));
	IOFreePhyMem(&(handle->mem_desc));

	free(handle);
	*instance = NULL;
}





void vpu_display(DecodingInstance dec)
{
	v4l_put_data(dec->disp, dec->rot_buf_count, V4L2_FIELD_NONE, 30);
}


static int vpu_decoding_thread(DecodingInstance dec)
{
	int ret;
	char timestring[256] = { };
	time_t rawtime;
	struct tm* timeinfo;
	text_layout text = NULL;
	unsigned char* frame = NULL;

	text = text_layout_create(280, 30);
	text_layout_set_font(text, "Liberation Mono", 24);

	dec->run_thread = 1;
	while(dec->run_thread)
	{
		ret = vpu_decode_one_frame(dec, &frame);
		if(!frame||ret == -1) continue;

		/* Draw OSD */

		if(dec->show_timestamp)
		{
			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(timestring, 255, "%p %l:%M:%S %Y/%m/%d", timeinfo);
			text_layout_render_markup_text(text, timestring);
			text_layout_copy_to_yuv422p(text, 50, 50, frame, dec->picwidth, dec->picheight);
		}

		queue_push(dec->output_queue, frame);
		vpu_display(dec);
	}

	text_layout_destroy(text);
	return 0;
}

void vpu_start_decoding(DecodingInstance dec)
{
	dec->output_queue = queue_new(dec->output_buffer_size, VPU_DECODING_QUEUE_SIZE);
	pthread_create(&dec->thread, NULL, (void*)vpu_decoding_thread, (void**)dec);
}

void vpu_stop_decoding(DecodingInstance dec)
{
	int ret;
	dec->run_thread = 0;
	pthread_join(dec->thread, (void**)&ret);
	queue_delete(&dec->output_queue);
}

queue vpu_get_decode_queue(DecodingInstance dec)
{
	return dec->output_queue;
}

void vpu_decoding_show_time_stamp(DecodingInstance dec, int bool)
{
	dec->show_timestamp = bool;
}
