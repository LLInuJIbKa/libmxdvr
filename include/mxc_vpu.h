/**
 * @file mxc_vpu.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 */
#ifndef MXC_VPU_H_
#define MXC_VPU_H_

#include "vpu_lib.h"
#include "v4l2dev.h"
#include "queue.h"

typedef struct EncodingInstance* EncodingInstance;
typedef struct DecodingInstance* DecodingInstance;

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

/**
 * @brief Start encoding thread.
 * @param instance Pointer to the instance
 * @param input Input queue
 */
void vpu_start_encoding(EncodingInstance instance, queue input);

/**
 * @brief Stop encoding thread.
 * @param instance Pointer to the instance
 */
void vpu_stop_encoding(EncodingInstance instance);

/**
 * @brief Create an instance object for decoding.
 * @param input Input queue
 * @retval DecodingInstance object
 */
DecodingInstance vpu_create_decoding_instance_for_v4l2(queue input);

/**
 * @brief Close and delete an instance object for decoding.
 * @param instance Pointer to the instance
 */
void vpu_close_decoding_instance(DecodingInstance* instance);

/**
 * @brief Decode a JPEG frame.
 * @param instance Pointer to the instance
 * @param output Pointer to output image
 */
int vpu_decode_one_frame(DecodingInstance instance, unsigned char** output);

/**
 * @brief Display decoded image.
 * @param instance Pointer to the instance
 */
void vpu_display(DecodingInstance instance);

/**
 * @brief Start decoding thread.
 * @param instance Pointer to the instance
 */
void vpu_start_decoding(DecodingInstance instance);

/**
 * @brief Stop decoding thread.
 * @param instance Pointer to the instance
 */
void vpu_stop_decoding(DecodingInstance instance);

/**
 * @brief Get output queue.
 * @param instance Pointer to the instance
 * @retval Output queue
 */
queue vpu_get_decode_queue(DecodingInstance instance);

#endif /* MXC_VPU_H_ */
