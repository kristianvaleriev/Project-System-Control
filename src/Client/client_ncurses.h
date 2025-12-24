#ifndef _CLIENT_NCURSES_H_
#define _CLIENT_NCURSES_H_

#include <ncurses.h>
#include <vterm.h>

void    make_panel(WINDOW** frame, WINDOW** pane,
                int rows, int cols, int startx, int starty);

void*   setup_client_ncurses(void*);
void    setup_ncurses(void);


void    render_vterm_diff(void);
size_t  vterm_write_to_input(char* bytes, size_t len);
void    setup_vterm(void);

#endif
