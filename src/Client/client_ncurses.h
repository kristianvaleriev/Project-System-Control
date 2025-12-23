#ifndef _CLIENT_NCURSES_H_
#define _CLIENT_NCURSES_H_

#include <ncurses.h>

void    setup_ncurses(void);
void*   setup_client_ncurses(void*);
void    make_panel(WINDOW** frame, WINDOW** pane,
                int rows, int cols, int startx, int starty);


#endif
