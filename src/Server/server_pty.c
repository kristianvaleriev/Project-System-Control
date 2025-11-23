#include "../../include/includes.h"
#include "../../include/utils.h"

#include <asm-generic/ioctls.h>
#include <fcntl.h>
#include <pty.h>
#include <termios.h>
#include <sys/ioctl.h>

#define TTY_PATH "/dev/tty"

void set_control_terminal(int term_fd, char* path)
{
    // Disconnect from the old controlling terminal
    int fd = open(TTY_PATH, O_RDWR | O_NOCTTY);
    if (fd >= 0) {
        ioctl(fd, TIOCNOTTY, NULL);
        close(fd);
    }

    if (setsid() < 0) 
        err_sys("setsid");
    if (open(TTY_PATH, O_RDWR | O_NOCTTY) >= 0) 
        err_quit_msg("still connected to controlling tty");

    if (ioctl(term_fd, TIOCSCTTY, NULL) < 0)
        err_sys("could not set tty as controlling terminal");
    
    if (open(path, O_RDWR) < 0)
        err_sys("open of control terminal path failed");

    fd = open(TTY_PATH, O_WRONLY);
    if (fd < 0)
        err_sys("could not open new controlling terminal");
    
    close(fd);
}
