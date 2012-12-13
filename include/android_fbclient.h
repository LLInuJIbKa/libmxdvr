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
 */
void android_fbclient_stop(void);

/**
 * @brief Report if fbclient is running or not.
 */
int android_fbclient_is_running(void);

/**
 * @brief Start device prober thread.
 * @details This thread monitors USB events from udev and detects if Android
 * devices are connected or not. Users have to modify <b>android_serial.txt</b>
 * to filter devices you want.
 */
void android_prober_start(void);

/**
 * @brief Stop device prober thread.
 */
void android_prober_stop(void);

/**
 * @brief Report if any device are connected.
 */
int android_is_device_connected(void);

/**
 * @brief Install fbserver into the Android device.
 */
void android_fbserver_install(void);

/**
 * @brief Start fbserver.
 */
void android_fbserver_start(void);

/**
 * @brief Stop fbserver.
 */
void android_fbserver_stop(void);


#endif /* ANDROID_FBCLIENT_H_ */
