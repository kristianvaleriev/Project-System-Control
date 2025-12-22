#include "../../include/includes.h"
#include "../../include/utils.h"

#include <ncurses.h>

void setup_ncurses(void)
{
    initscr();
    noecho();
    raw();

    atexit((void(*)(void)) endwin);
}

void make_panel(WINDOW** frame, WINDOW** pane,
                int rows, int cols, int starty, int startx)
{
    *frame = newwin(rows, cols, starty, startx);
    *pane  = derwin(*frame, rows - 1, cols - 1, 1, 1);

    if (!*frame || !*pane) 
        err_cont(0, "window creating failed");

    scrollok(*pane, TRUE);
}

int setup_client_ncurses(void)
{
    setup_ncurses();

    size_t n_rows = LINES * 0.25;
    WINDOW* frame, *pane;
    make_panel(&frame, &pane, n_rows, COLS, LINES - n_rows, 0);

    wborder(frame, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');
    wrefresh(frame);

    wgetch(pane);
    getch();

    return 0;
}
