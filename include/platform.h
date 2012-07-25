/**
 * @file platform.h
 * @author Ruei-Yuan Lu (RueiYuan.Lu@gmail.com)
 * @brief Collection of platform related functions
 */
#ifndef PLATFORM_H_
#define PLATFORM_H_

#define USE_NEONCPY

/**
 * @brief Show or hide cursor in specified tty device.
 * @param tty_path Path to the tty device node
 * @param bool	@li@c true visible
 * 		@li@c false invisible
 */
void tty_set_cursor_visible(const char* tty_path, const int bool);

/**
 * @brief Wake up the framebuffer.
 * @param tty_path Path to the tty device node
 */
void fb_wakeup(const char* tty_path);

/**
 * @brief Memory copy with NEON instructions
 * @param dest Destination
 * @param src Source
 * @param n Size to copy
 */
void neoncpy(void *dest, const void *src, size_t n);

#ifdef USE_NEONCPY
#define MEMCPY(x, y, z) neoncpy(x, y, z)
#else
#define MEMCPY(x, y, z) memcpy(x, y, z)
#endif

#endif /* PLATFORM_H_ */
