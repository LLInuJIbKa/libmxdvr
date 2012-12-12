/**
 * @file android_fbclient.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 * @brief Framebuffer client API
 * @details Use these functions to receive and display JPEG compressed framebuffer data from remote Android phone.
 * @include fbdemo.c Demo program
 */

#ifndef ANDROID_FBCLIENT_H_
#define ANDROID_FBCLIENT_H_


/**
 * @brief Start framebuffer client thread.
 * @details Connect to remote Android phone and receive JPEG image, then decode and display by VPU.
 */
void android_fbclient_start(void);

/**
 * @brief Stop framebuffer client thread.
 * @details Set stop flag to make thread exits.
 */
void android_fbclient_stop(void);

/**
 * @brief Start device prober thread.
 * @details This thread monitors USB events from udev and detects if Android devices are connected or not.
 */
void android_prober_start(void);

/**
 * @brief Stop device prober thread.
 * @details Set stop flag to make thread exits.
 */
void android_prober_stop(void);

/**
 * @brief Report if any device are connected.
 * @details Return true if an Android device is connected.
 */
int android_is_device_connected(void);



#endif /* ANDROID_FBCLIENT_H_ */
