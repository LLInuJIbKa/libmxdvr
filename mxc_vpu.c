#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "vpu_lib.h"
#include "vpu_io.h"
#include "mxc_vpu.h"
#include "framebuf.h"

#define STREAM_ENC_PIC_RESET (1)

/* mxc_vpu_test: enc.c start; modified by Ruei-Yuan Lu */

typedef unsigned long u32;

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

static int vpu_write(int fd, char *vptr, int n)
{
	int nleft;
	int nwrite;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while(nleft > 0)
	{
		if((nwrite = write(fd, ptr, nleft)) <= 0)
		{
			perror("fwrite: ");
			return (-1); /* error */
		}

		nleft -= nwrite;
		ptr += nwrite;
	}

	return (n);
}

static int encoder_open(struct EncodingInstance *instance)
{
	EncHandle handle = NULL;
	EncOpenParam encop = {};
	RetCode ret;

	/* Fill up parameters for encoding */
	encop.bitstreamBuffer = instance->phy_bsbuf_addr;
	encop.bitstreamBufferSize = 0x200000;
	encop.bitstreamFormat = STD_AVC;

	/* Please change encoded picture width and height per your needs
	 it is same as source picture image normally */
	instance->enc_picwidth = instance->src_picwidth;
	instance->enc_picheight = instance->src_picheight;
	instance->input_size = instance->src_picwidth * instance->src_picheight * 3 / 2;

	/* If rotation angle is 90 or 270, pic width and height are swapped */
	encop.picWidth = instance->enc_picwidth;
	encop.picHeight = instance->enc_picheight;

	/*Note: Frame rate cannot be less than 15fps per H.263 spec */
	encop.frameRateInfo = 15;
	encop.bitRate = 0;
	encop.gopSize = 0;
	encop.slicemode.sliceMode = 0; /* 0: 1 slice per picture; 1: Multiple slices per picture */
	encop.slicemode.sliceSizeMode = 0; /* 0: silceSize defined by bits; 1: sliceSize defined by MB number*/
	encop.slicemode.sliceSize = 4000; /* Size of a slice in bits or MB numbers */

	encop.initialDelay = 0;
	encop.vbvBufferSize = 0; /* 0 = ignore 8 */
	encop.intraRefresh = 0;
	encop.sliceReport = 0;
	encop.mbReport = 0;
	encop.mbQpReport = 0;
	encop.rcIntraQp = -1;
	encop.userQpMax = 0;
	encop.userQpMin = 0;
	encop.userQpMinEnable = 0;
	encop.userQpMaxEnable = 0;

	encop.userGamma = (Uint32)(0.75 * 32768); /*  (0*32768 <= gamma <= 1*32768) */
	encop.RcIntervalMode = 1; /* 0:normal, 1:frame_level, 2:slice_level, 3: user defined Mb_level */
	encop.MbInterval = 0;
	encop.avcIntra16x16OnlyModeEnable = 0;

	encop.ringBufferEnable = 0;
	encop.dynamicAllocEnable = 0;
	encop.chromaInterleave = 0;

	encop.EncStdParam.avcParam.avc_constrainedIntraPredFlag = 0;
	encop.EncStdParam.avcParam.avc_disableDeblk = 1;
	encop.EncStdParam.avcParam.avc_deblkFilterOffsetAlpha = 6;
	encop.EncStdParam.avcParam.avc_deblkFilterOffsetBeta = 0;
	encop.EncStdParam.avcParam.avc_chromaQpOffset = 10;
	encop.EncStdParam.avcParam.avc_audEnable = 0;
	encop.EncStdParam.avcParam.avc_fmoEnable = 0;
	encop.EncStdParam.avcParam.avc_fmoType = 0;
	encop.EncStdParam.avcParam.avc_fmoSliceNum = 1;
	encop.EncStdParam.avcParam.avc_fmoSliceSaveBufSize = 32; /* FMO_SLICE_SAVE_BUF_SIZE */

	ret = vpu_EncOpen(&handle, &encop);
	if(ret != RETCODE_SUCCESS)
	{
		return -1;
	}

	instance->handle = handle;
	return 0;
}

static int encoder_configure(struct EncodingInstance* instance)
{
	EncHandle handle = instance->handle;
	EncInitialInfo initinfo = {};
	RetCode ret;

	ret = vpu_EncGetInitialInfo(handle, &initinfo);
	if(ret != RETCODE_SUCCESS)
	{
		fputs("Encoder GetInitialInfo failed\n", stderr);
		return -1;
	}

	instance->fbcount = instance->src_fbid = initinfo.minFrameBufferCount;
	instance->mbInfo.enable = 0;
	instance->mvInfo.enable = 0;
	instance->sliceInfo.enable = 0;

	return 0;
}

static int encoder_allocate_framebuffer(struct EncodingInstance* instance)
{
	EncHandle handle = instance->handle;
	int i, enc_stride, src_stride, fbcount = instance->fbcount, src_fbid = instance->src_fbid;
	RetCode ret;
	FrameBuffer *fb;
	struct frame_buf **pfbpool;

	fb = instance->fb = calloc(fbcount + 1, sizeof(FrameBuffer));
	if(fb == NULL)
	{
		fputs("Failed to allocate enc->fb\n", stderr);
		return -1;
	}

	pfbpool = instance->pfbpool = calloc(fbcount + 1, sizeof(struct frame_buf*));
	if(pfbpool == NULL)
	{
		fputs("Failed to allocate enc->pfbpool", stderr);
		free(fb);
		return -1;
	}

	for(i = 0; i < fbcount; i++)
	{
		pfbpool[i] = framebuf_alloc(STD_AVC, MODE420, (instance->enc_picwidth + 15) & ~15, (instance->enc_picheight + 15) & ~15);

		if(pfbpool[i] == NULL)
		{
			fbcount = i;
			goto err1;
		}

		fb[i].bufY = pfbpool[i]->addrY;
		fb[i].bufCb = pfbpool[i]->addrCb;
		fb[i].bufCr = pfbpool[i]->addrCr;
		fb[i].strideY = pfbpool[i]->strideY;
		fb[i].strideC = pfbpool[i]->strideC;
	}

	/* Must be a multiple of 16 */

	enc_stride = (instance->enc_picwidth + 15) & ~15;
	src_stride = (instance->src_picwidth + 15) & ~15;

	ret = vpu_EncRegisterFrameBuffer(handle, fb, fbcount, enc_stride, src_stride);
	if(ret != RETCODE_SUCCESS)
	{
		fputs("Register frame buffer failed", stderr);
		goto err1;
	}

	pfbpool[src_fbid] = framebuf_alloc(STD_AVC, MODE420, instance->src_picwidth, instance->src_picheight);

	if(pfbpool[src_fbid] == NULL)
	{
		goto err1;
	}

	fb[src_fbid].bufY = pfbpool[src_fbid]->addrY;
	fb[src_fbid].bufCb = pfbpool[src_fbid]->addrCb;
	fb[src_fbid].bufCr = pfbpool[src_fbid]->addrCr;
	fb[src_fbid].strideY = pfbpool[src_fbid]->strideY;
	fb[src_fbid].strideC = pfbpool[src_fbid]->strideC;
	instance->fbcount++;

	return 0;

	err1: for(i = 0; i < fbcount; i++)
	{
		framebuf_free(pfbpool[i]);
	}

	free(fb);
	free(pfbpool);
	return -1;
}

static int encoder_fill_headers(struct EncodingInstance* instance)
{
	EncHeaderParam enchdr_param = {};
	EncHandle handle = instance->handle;
	int ret;

#if STREAM_ENC_PIC_RESET == 1
	u32 vbuf;
	u32 phy_bsbuf = instance->phy_bsbuf_addr;
	u32 virt_bsbuf = instance->virt_bsbuf_addr;
#endif

	/* Must put encode header before encoding */
	enchdr_param.headerType = SPS_RBSP;
	vpu_EncGiveCommand(handle, ENC_PUT_AVC_HEADER, &enchdr_param);

#if STREAM_ENC_PIC_RESET == 1
	vbuf = (virt_bsbuf + enchdr_param.buf - phy_bsbuf);
	ret = vpu_write(instance->fd, (void*)vbuf, enchdr_param.size);
	if(ret < 0)
		return -1;
#endif
	enchdr_param.headerType = PPS_RBSP;
	vpu_EncGiveCommand(handle, ENC_PUT_AVC_HEADER, &enchdr_param);

#if STREAM_ENC_PIC_RESET == 1
	vbuf = (virt_bsbuf + enchdr_param.buf - phy_bsbuf);
	ret = vpu_write(instance->fd, (void*)vbuf, enchdr_param.size);
	if(ret < 0)
		return -1;
#endif
	return 0;
}

/* mxc_vpu_test: enc.c end */

int vpu_init(void)
{
	vpu_versioninfo ver;

	framebuf_init();
	vpu_Init(NULL);
	vpu_GetVersionInfo(&ver);

	fprintf(stderr, "VPU firmware version: %d.%d.%d\n", ver.fw_major, ver.fw_minor, ver.fw_release);
	fprintf(stderr, "VPU library version: %d.%d.%d\n", ver.lib_major, ver.lib_minor, ver.lib_release);
	return 0;
}

void vpu_uninit(void)
{
	vpu_UnInit();
}

EncodingInstance vpu_create_encoding_instance(const int src_width, const int src_height, const char* filename)
{
	EncodingInstance instance = NULL;

	instance = calloc(1, sizeof(struct EncodingInstance));

	instance->mem_desc.size = 0x200000;
	IOGetPhyMem(&(instance->mem_desc));
	instance->virt_bsbuf_addr = IOGetVirtMem(&(instance->mem_desc));
	instance->phy_bsbuf_addr = instance->mem_desc.phy_addr;
	instance->src_picwidth = src_width;
	instance->src_picheight = src_height;
	instance->fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);

	encoder_open(instance);
	encoder_configure(instance);
	encoder_allocate_framebuffer(instance);
	encoder_fill_headers(instance);

	instance->enc_param.sourceFrame = &instance->fb[instance->src_fbid];
	instance->enc_param.quantParam = 23;
	instance->enc_param.forceIPicture = 0;
	instance->enc_param.skipPicture = 0;
	instance->enc_param.enableAutoSkip = 1;

	instance->enc_param.encLeftOffset = 0;
	instance->enc_param.encTopOffset = 0;

	return instance;
}

int vpu_encode_one_frame(EncodingInstance instance, const unsigned char* data)
{
	EncHandle handle = instance->handle;
	int src_fbid = instance->src_fbid;
	EncOutputInfo outinfo =	{};
	int ret;
	u32 vbuf;
	u32 yuv_addr;

	yuv_addr = instance->pfbpool[src_fbid]->addrY + instance->pfbpool[src_fbid]->desc.virt_uaddr - instance->pfbpool[src_fbid]->desc.phy_addr;
	memcpy((unsigned char*)yuv_addr, data, instance->input_size);

	ret = vpu_EncStartOneFrame(handle, &(instance->enc_param));

	while(vpu_IsBusy())
		vpu_WaitForInt(200);

	ret = vpu_EncGetOutputInfo(handle, &outinfo);

#if STREAM_ENC_PIC_RESET == 1
	vbuf = (instance->virt_bsbuf_addr + outinfo.bitstreamBuffer - instance->phy_bsbuf_addr);
	ret = vpu_write(instance->fd, (void*)vbuf, outinfo.bitstreamSize);
#endif
	return 0;
}

void vpu_close_encoding_instance(EncodingInstance* instance)
{
	int i;
	EncOutputInfo outinfo =	{};
	RetCode ret;
	EncodingInstance ptr = *instance;

	for(i = 0; i < ptr->fbcount; i++)
	{
		framebuf_free(ptr->pfbpool[i]);
	}

	free(ptr->fb);
	free(ptr->pfbpool);

	ret = vpu_EncClose(ptr->handle);

	if(ret == RETCODE_FRAME_NOT_COMPLETE)
	{
		vpu_EncGetOutputInfo(ptr->handle, &outinfo);
		vpu_EncClose(ptr->handle);
	}
	IOFreeVirtMem(&(ptr->mem_desc));
	IOFreePhyMem(&(ptr->mem_desc));
	free(*instance);

	*instance = NULL;
}

