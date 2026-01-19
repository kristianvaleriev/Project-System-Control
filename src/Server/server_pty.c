#include "../../include/includes.h"
#include "../../include/utils.h"
#include "../../include/network.h"

#include <pty.h>
#include <fcntl.h>
#include <errno.h>
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


int set_win_size(int client_socket, int term_fd, struct client_request* req)
{
    struct winsize wins;
    if (recv(client_socket, &wins, req->data_size, 0) < 0) {
        info_msg("could not retrive winsize struct");
        return errno;
    }

    if (ioctl(term_fd, TIOCSWINSZ, &wins) < 0)
        err_sys("ioctl");

    info_msg("terminal window's size set to %d rows and %d cols", 
             wins.ws_row, wins.ws_col);

    return 0;
}

