#ifndef _CLIENT_NCURSES_H_
#define _CLIENT_NCURSES_H_

#include <ncurses.h>
#include <vterm.h>

#define LOG_NAME "client.log"

struct window_node;

enum win_type {
    BASIC,    // Just a window without any special properties.
    MAIN,     // The main window of the whole program. If more than one, the rest are ignored.
    FRAME,    // The frame window of a pane window. It's data points to it's pane.
    PANE,     // Subwindows of the main window.
    
    WIN_TYPE_COUNT,
};

struct window_placement {
    int fd;
    enum win_type type;

    struct coords {
        int rows;
        int cols;

        int starty;
        int startx;
    } coords;

    chtype border[8];

    struct window_node* winn;
};

#define for_each_win(var, arg, count) \
    for (struct window_placement* var = arg; var - arg < count; var++)

pid_t   handle_ncurses_and_fork(struct window_placement*, size_t);
void*   setup_client_ncurses(struct window_placement*, size_t);
void    init_ncurses(void);

void    make_panel(WINDOW** frame, WINDOW** pane, 
                   struct window_placement* place);

void    render_vterm_diff(void* vt, WINDOW* display);
void*   initialize_vterm(WINDOW*);

#endif
