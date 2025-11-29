#include "../../include/includes.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <errno.h>
/*
 * Client functionality for configuring user's terminal, which is used for user's I/O
 *
 * We have to set the terminal to raw mode so that we can catch every type of input character
 * and send it directly to the server, without it getting processed by the terminal's line discipline
 *
 * Definiton of raw mode: 
 *  - Noncanonical mode: Every character is send to us when is entered, not when a line is made.
 *  - Echo off: Our therminal won't display the characters we enter by the keyboard.
 *  - One byte at a time: MIN=1, TIME=0
*/

static struct termios users_term;
static int is_raw   = 0;
static int saved_fd = -1;

// Set's errno to failer status and returns -1 on failer
int set_tty_raw(int fd) 
{
    if (!isatty(fd) || is_raw) {
        errno = EINVAL;
        return -1;
    }

    struct termios term;
    if (tcgetattr(fd, &term) < 0) {
        return -1;
    }
    users_term = term;


   	term.c_iflag |= IGNPAR;
    // No SIGINT on BREAK, CR-to-NL map off, input parity off,
    // dont strip 8th bit on input (no idea why, really),
    // output flow control off. 
    //term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    term.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF);

#ifdef IUCLC
    term.c_iflag &= ~IUCLC;
#endif
    
    // Echo off, canonical mode off, extended proc off,
    // signal chars off.
    //term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    term.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
#ifdef IEXTEN
    term.c_lflag &= ~IEXTEN;
#endif

    // Clear size bits and parity checking 
    //term.c_cflag &= ~(CSIZE | PARENB);  
    //term.c_cflag |= CS8;

    // Output processing off.
    term.c_oflag &= ~(OPOST);

    term.c_cc[VMIN]  = 1;
    term.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSAFLUSH, &term) < 0)
        return -1;

    // Verify our changes, because tcsetattr can return success on partial 
    // changes, because of course it can
    /*
    tcgetattr(fd, &term);
    if ((term.c_lflag & (ECHO | ICANON | IEXTEN | ISIG)) ||
        (term.c_iflag & (BRKINT | ICRNL | INPCK | ISTRIP | IXON)) ||
        (term.c_cflag & (CSIZE | PARENB | CS8)) != CS8 ||
        (term.c_oflag & OPOST) ||  term.c_cc[VMIN] != 1 ||
         term.c_cc[VTIME] != 0) {
        // Only some of the changes were made. Restore the
        // original settings.
        tcsetattr(fd, TCSAFLUSH, &users_term);
        errno = EINVAL;
        return -1;
    }
    */

    is_raw = 1;
    saved_fd = fd;
    return 0;
}

int set_tty_user(int fd)
{
    if (!is_raw)
        return 0;

    if (tcsetattr(fd, TCSAFLUSH, &users_term) != 0)
        return -1;

    is_raw = 0;
    return 0;
}

void set_tty_atexit(void)
{
    if (saved_fd >= 0)
        set_tty_user(saved_fd);
}
