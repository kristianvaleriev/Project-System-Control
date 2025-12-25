#ifndef _CLIENT_NCURSES_H_
#define _CLIENT_NCURSES_H_

#include <ncurses.h>
#include <vterm.h>

pid_t   handle_ncurses_and_fork(void);
void*   setup_client_ncurses(void*);
void    setup_ncurses(void);

void    make_panel(WINDOW** frame, WINDOW** pane, chtype border[8],
                int rows, int cols, int startx, int starty);

size_t  vterm_write_to_input(char* bytes, size_t len);
void    render_vterm_diff(WINDOW* display);
void    setup_vterm(void);

#endif
