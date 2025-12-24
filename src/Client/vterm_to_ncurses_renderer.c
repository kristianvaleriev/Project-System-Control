#ifndef WITHOUT_NCURSES

#include "../../include/includes.h"
#include "../../include/utils.h"

#include <ncurses.h>
#include <vterm.h>

static VTerm* vt;
static VTermScreen* vts;

// Represents one cell in our snapshot
typedef struct {
    char ch;
    short fg;
} Cell;

// A global snapshot pointer:
static Cell *prev_screen = NULL;

// Size of last snapshot
static int prev_rows = 0;
static int prev_cols = 0;

/*
 * Renders only changed cells from vterm to ncurses,
 * and positions the ncurses cursor to the vterm cursor.
 */
void render_vterm_diff(void) 
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // Reallocate snapshot if size has changed
    if (!prev_screen || rows != prev_rows || cols != prev_cols) {
        free(prev_screen);
        prev_rows = rows;
        prev_cols = cols;
        prev_screen = calloc(rows * cols, sizeof(Cell));
    }

    // Temporary local buffer before applying updates
    VTermScreenCell cell;

    // For cursor placement later
    VTermState* vstate = vterm_obtain_state(vt);
    VTermPos vpos;
    vterm_state_get_cursorpos(vstate, &vpos);

    for (int r = 0; r < (rows - rows * 0.25); r++) 
    {
        for (int c = 0; c < cols; c++) 
        {
            if (!vterm_screen_get_cell(vts, (VTermPos){r, c}, &cell))
                continue;

            char ch   = cell.chars[0] ? cell.chars[0] : ' ';
            short fg  = -1;  // indexed color (0..255)

            // Get snapshot index
            int idx = r * cols + c;

            // Test if this cell changed
            if (prev_screen[idx].ch != ch ||
                prev_screen[idx].fg != fg) {

                prev_screen[idx].ch = ch;
                prev_screen[idx].fg = fg;

                // Set color if needed (optional)
                if (fg >= 0) {
                    attron(COLOR_PAIR(fg + 1));
                }

                // Draw to ncurses
                mvaddch(r, c, ch);

                if (fg >= 0) {
                    attroff(COLOR_PAIR(fg + 1));
                }
            }
        }
    }

    move(vpos.row, vpos.col);

    refresh();
}

void render_vterm_to_ncurses(void)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    for (int i = 0; i < rows * 0.25; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            VTermScreenCell cell;
            if (!vterm_screen_get_cell(vts, (VTermPos) { .row = i, .col = j}, &cell))
                continue;

            chtype ch = cell.chars[0] ? cell.chars[0] : ' ';
            //if (cell.fg.type == VTERM_COLOR_RGB)
                //attron(COLOR_PAIR(1));

            //info_msg("Getting char: %c-%d; pos: %d %d", ch, ch, i, j);
            mvaddch(i, j, ch);
            //attroff(COLOR_PAIR(1));
        }
    }

    refresh();
}

size_t vterm_write_to_input(char* bytes, size_t len) 
{ 
    return vterm_input_write(vt, bytes, len); 
}

static int damage_callback(VTermRect rect, void* user) {return 1;}
static VTermScreenCallbacks screen_cb = {
    .damage = damage_callback,
};

void setup_vterm(void)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    vt = vterm_new(rows, cols);
    if (!vt) 
        err_quit_msg(0, "vterm_new failed");
    
    vterm_set_utf8(vt, 1);

    vts = vterm_obtain_screen(vt);
    vterm_screen_set_callbacks(vts, NULL, NULL);
    vterm_screen_enable_altscreen(vts, 1);
    vterm_screen_reset(vts, 1);
}

#endif
