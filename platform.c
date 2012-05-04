#include <stdio.h>
#include "platform.h"

/**
 * @brief Show or hide cursor in specified tty device.
 * @param tty_path Path to the tty device node
 * @param bool	@true visible
 * 		@false invisible
 */
void tty_set_cursor_visible(const char* tty_path, const int bool)
{
	FILE* fp = fopen(tty_path, "w");
	if(bool)
		fputs("\e[?25h", fp);
	else	fputs("\e[?25l", fp);
	fclose(fp);
}


void fb_wakeup(const char* tty_path)
{
	FILE* fp = fopen(tty_path, "w");
	fputs("\033[9]", fp);
	fclose(fp);
}
