#ifndef __TTY_CONF_H__
#define __TTY_CONF_H__

int   set_tty_raw (int fd);
int   set_tty_user(int fd);
void  set_tty_atexit(void);

#endif
