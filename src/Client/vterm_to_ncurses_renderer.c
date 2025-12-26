#ifndef WITHOUT_NCURSES

#include "../../include/includes.h"
#include "../../include/utils.h"

#include <ncurses.h>
#include <vterm.h>

VTerm* vt;
VTermScreen* vts;

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


static short vterm_color_to_ncurses(const VTermColor *c);
static short get_pair(short fg, short bg);
static void apply_attrs(VTermScreenCell *cell);

#define MAX_PAIRS 1024
typedef struct {
    short fg;
    short bg;
} ColorPair;

static ColorPair pair_cache[MAX_PAIRS];
static int pair_count = 1;  // pair 0 is reserved


/*
 * Renders only changed cells from vterm to ncurses,
 * and positions the ncurses cursor to the vterm cursor.
 */
void render_vterm_diff(WINDOW* display) 
{
    int rows, cols;
    getmaxyx(display, rows, cols);

    // Reallocate snapshot if size has changed
    if (!prev_screen || rows != prev_rows || cols != prev_cols) {
        free(prev_screen);
        prev_rows = rows;
        prev_cols = cols;
        prev_screen = calloc(rows * cols, sizeof(Cell));

        vterm_set_size(vt, rows, cols);
    }

    // Temporary local buffer before applying updates
    VTermScreenCell cell;

    for (int r = 0; r < rows; r++) 
    {
        for (int c = 0; c < cols; c++) 
        {
            if (!vterm_screen_get_cell(vts, (VTermPos){r, c}, &cell))
                continue;

            char  ch = cell.chars[0] ? cell.chars[0] : ' ';
            short fg = cell.fg.indexed.idx;

            // Get snapshot index
            int idx = r * cols + c;

            // Test if this cell changed
            if (prev_screen[idx].ch != ch ||
                prev_screen[idx].fg != fg) {

                prev_screen[idx].ch = ch;
                prev_screen[idx].fg = fg;

                wattrset(display, A_NORMAL);

                short fg = vterm_color_to_ncurses(&cell.fg);
                short bg = vterm_color_to_ncurses(&cell.bg);

                short pair = get_pair(fg, bg);
                if (pair > 0)
                    wattron(display, COLOR_PAIR(pair));

                apply_attrs(&cell);
                mvwaddch(display, r, c, ch);

                if (pair > 0)
                    wattroff(display, COLOR_PAIR(pair));
            }
        }
    }

    VTermState* vstate = vterm_obtain_state(vt);
    VTermPos vpos;
    vterm_state_get_cursorpos(vstate, &vpos);

    wmove(display, vpos.row, vpos.col);
    wrefresh(display);
}

static int rgb_to_256(uint8_t r, uint8_t g, uint8_t b) 
{
    if (r == g && g == b) {
        if (r < 8) return 16;
        if (r > 248) return 231;
        return 232 + (r - 8) / 10;
    }

    int rc = r * 5 / 255;
    int gc = g * 5 / 255;
    int bc = b * 5 / 255;
    return 16 + 36 * rc + 6 * gc + bc;
}

static short vterm_color_to_ncurses(const VTermColor *c)
{
    switch (c->type) {
    case VTERM_COLOR_INDEXED:
        return c->indexed.idx;

    case VTERM_COLOR_RGB:
        return rgb_to_256(
            c->rgb.red,
            c->rgb.green,
            c->rgb.blue
        );
    }

    return -1;
}

static void apply_attrs(VTermScreenCell *cell) 
{
    attr_t a = A_NORMAL;

    if (cell->attrs.bold)      
        a |= A_BOLD;

    if (cell->attrs.underline)
        a |= A_UNDERLINE;

    if (cell->attrs.reverse) 
        a |= A_REVERSE;

    attron(a);
}

static short get_pair(short fg, short bg)
{
    for (int i = 1; i < pair_count; i++) {
        if (pair_cache[i].fg == fg &&
            pair_cache[i].bg == bg)
            return i;
    }

    if (pair_count >= MAX_PAIRS)
        return 0;

    init_pair(pair_count, fg, bg);
    pair_cache[pair_count] = (ColorPair){ fg, bg };
    return pair_count++;
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
