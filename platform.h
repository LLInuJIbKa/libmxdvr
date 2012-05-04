#ifndef PLATFORM_H_
#define PLATFORM_H_


void tty_set_cursor_visible(const char* tty_path, const int bool);
void fb_wakeup(const char* tty_path);



#endif /* PLATFORM_H_ */
