/**
 * @file mxc_vpu.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 */
#ifndef MXC_VPU_H_
#define MXC_VPU_H_

#include "vpu_lib.h"
#include "v4l2dev.h"

typedef struct EncodingInstance* EncodingInstance;
typedef struct DecodingInstance* DecodingInstance;

typedef enum
{
	FILENAME,
	MEMORYBLOCK
}InputType;


/**
 * @brief Initialize VPU.
 */
int vpu_init(void);

/**
 * @brief Uninitialize VPU.
 */
void vpu_uninit(void);

/**
 * @brief Create an instance object for encoding.
 * @param src_width Width of source image
 * @param src_height Height of source image
 * @retval EncodingInstance object
 */
EncodingInstance vpu_create_encoding_instance(const int src_width, const int src_height, const char* filename);

/**
 * @brief Encode one frame and write to file.
 * @param instance EncodingInstance object
 * @param data Frame data
 */
int vpu_encode_one_frame(EncodingInstance instance, const unsigned char* data);

/**
 * @brief Close and delete an instance object for encoding.
 * @param instance Pointer to the instance
 */
void vpu_close_encoding_instance(EncodingInstance* instance);


DecodingInstance vpu_create_decoding_instance_for_v4l2(v4l2dev device);
int vpu_decode_one_frame(DecodingInstance instance, unsigned char* output);

#endif /* MXC_VPU_H_ */
