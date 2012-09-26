/**
 * @file fbclient.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 * @brief Framebuffer client API
 * @details Use these functions to receive and display JPEG compressed framebuffer data from remote Android phone.
 * @include fbdemo.c Demo program
 */

#ifndef FBCLIENT_H_
#define FBCLIENT_H_


/**
 * @brief Start framebuffer client thread.
 * @details Connect to remote Android phone and receive JPEG image, then decode and display by VPU.
 */
void fbclient_start(void);

/**
 * @brief Stop framebuffer client thread.
 * @details Set stop flag to make thread exits.
 */
void fbclient_stop(void);

#endif /* FBCLIENT_H_ */
