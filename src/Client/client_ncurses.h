#ifndef _CLIENT_NCURSES_H_
#define _CLIENT_NCURSES_H_

#include <ncurses.h>
#include <vterm.h>

pid_t   handle_ncurses_and_fork(void);
void*   setup_client_ncurses(void*);
void    setup_ncurses(WINDOW** win);

void    make_panel(WINDOW** frame, WINDOW** pane, chtype border[8],
                int rows, int cols, int startx, int starty);

void    render_vterm_diff(void* vt, WINDOW* display);
void*   initialize_vterm(WINDOW*);

#endif
