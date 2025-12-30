#ifndef _CLIENT_NCURSES_H_
#define _CLIENT_NCURSES_H_

#include <ncurses.h>
#include <vterm.h>

#define LOG_NAME "client.log"

enum win_type {
    BASIC,    // Just a window without any special properties.
    MAIN,     // The main window of the whole program. If more than one, the rest are ignored.
    FRAME,    // The frame window of a pane window. It's data points to it's pane.
    PANE,     // Subwindows of the main window.
};

struct window_placement {
    int fd;
    enum win_type type;

    struct coords {
        int rows;
        int cols;

        int startx;
        int starty;
    } coords;

    chtype border[8];
};

pid_t   handle_ncurses_and_fork(struct window_placement*);
void*   setup_client_ncurses(struct window_placement*);
void    init_ncurses(WINDOW** win);

void    make_panel(WINDOW** frame, WINDOW** pane, chtype border[8],
                int rows, int cols, int startx, int starty);

void    render_vterm_diff(void* vt, WINDOW* display);
void*   initialize_vterm(WINDOW*);

#endif
