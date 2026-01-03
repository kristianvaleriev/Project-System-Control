#ifndef WITHOUT_NCURSES

#include "../../include/includes.h"
#include "../../include/utils.h"

#include <sys/wait.h>
#include <ncurses.h>
#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <vterm.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <pty.h>

#include "client_ncurses.h"


/*
int is_setup_done = 0; // Probably not needed, but just in case.

pthread_cond_t  setup_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t setup_lock = PTHREAD_MUTEX_INITIALIZER;
*/ 

static WINDOW* main_win;

typedef struct {
    WINDOW* frame;
    chtype border_chs[8];
} win_pane_t;

struct window_node {
    WINDOW* win;
    int type;

    void* data;
    void* vt;
}* win_array;

static size_t win_arr_count;
static size_t win_arr_alloced;

static void init_win_array(int n)
{
    win_array = calloc(n, sizeof *win_array);
    for (int i = 0; i < n; i++) 
    {
        win_array[i].type = BASIC;
        win_array[i].data = NULL;
        win_array[i].win  = NULL;
        win_array[i].vt   = NULL;
    }

    win_arr_count = 0;
    win_arr_alloced = n;
}

static size_t insert_into_windows(WINDOW* win, int type, void* data, struct window_placement* place)
{
    size_t idx = win_arr_count++;

    if (win_arr_count >= win_arr_alloced) {
        win_arr_alloced += 2;
        win_array = realloc(win_array, win_arr_alloced * sizeof *win_array);

        if (!win_array) 
            err_sys("realloc failed!");
    }

    win_array[idx].win = win;
    win_array[idx].type = type;
    win_array[idx].data = data;
    
    if (place) 
        place->winn = &win_array[idx];

    return idx;
}
#define insert_into_windows_def(win, place) \
        insert_into_windows(win, 0, NULL, place)

static void refresh_windows(void)
{
    for (size_t i = 0; i < win_arr_count; i++) 
    {
        if (win_array[i].type == PANE)
            touchwin(((win_pane_t*) win_array[i].data)->frame);
        wrefresh(win_array[i].win);
    }
}

static void clear_windows(void)
{
    for (size_t i = 0; i < win_arr_count; i++)
        wclear(win_array[i].win);
    refresh_windows();
}

WINDOW* setup_window(struct window_placement* new_win)
{
    struct coords c = new_win->coords;
    struct coords empty = {0};
    if (!memcmp(&c, &empty, sizeof empty))
        getmaxyx(stdscr, c.rows, c.cols);

    WINDOW* win = newwin(c.rows, c.cols, c.startx, c.starty);
    if (!win) 
        err_quit_msg("could not create new window");

    wattrset(win, A_NORMAL);
    idlok(win, FALSE);
    scrollok(win, FALSE);
    leaveok(win, TRUE);

    insert_into_windows_def(win, new_win);

    return win;
}

void init_ncurses(void)
{
    initscr();
    curs_set(0);
    noecho();
    raw();
    nodelay(stdscr, 1);

    start_color();
    use_default_colors();

    for (int color_pair = 0; color_pair <= 255; color_pair++)
        init_pair(color_pair + 1, color_pair, -1);

    atexit((void(*)(void)) endwin);
   
    insert_into_windows_def(stdscr, NULL);
}

void display_panel_border(struct window_node* winn)
{
    if (winn->type != PANE)
        return;

    assert(winn->win  != NULL);
    assert(winn->data != NULL);

    win_pane_t* pane_data = winn->data;
    chtype* border_chs = pane_data->border_chs;

    wborder(pane_data->frame, border_chs[0], border_chs[1], 
            border_chs[2], border_chs[3], border_chs[4], 
            border_chs[5], border_chs[6], border_chs[7]);
}

int begin_row = 1, begin_col = 1;

void make_panel(WINDOW** ret_frame, WINDOW** ret_pane, struct window_placement* place)
{
    WINDOW* frame, *pane;

    chtype* border = place->border;

    struct coords c = place->coords;
    int rows = c.rows;
    int cols = c.cols;
    int starty = c.starty;
    int startx = c.startx;

    frame = newwin(rows, cols, starty, startx);
    if (!frame)
        err_quit_msg("frame window creation failed");

    pane = derwin(frame, rows - 1, cols - 1, begin_row, begin_col);
    if (!pane)
        err_quit_msg("pane window creation failed");

    int maxy, maxx;
    if (main_win) {
        getmaxyx(main_win, maxy, maxx);
        wresize(main_win, maxy - rows - 1, maxx);
    }
    else {
        getmaxyx(stdscr, maxy, maxx);
        wresize(stdscr, maxy - rows - 1, maxx);
    }

    win_pane_t* new_pane = malloc(sizeof *new_pane);
    new_pane->frame = frame;
    memcpy(new_pane->border_chs, border, 8 * sizeof *border);

    size_t idx;
    idx = insert_into_windows(frame, FRAME, &win_array[win_arr_count], NULL);
    idx = insert_into_windows(pane, PANE, new_pane, place);

    display_panel_border(&win_array[idx]);

    if (ret_frame) *ret_frame = frame;
    if (ret_pane)  *ret_pane = pane;
}


static volatile sig_atomic_t ncurse_resize = 0;
static void (*client_winch_handle)(int);

static void sig_winch_handle(int signo)
{
    ncurse_resize = 1;
    if (client_winch_handle)
        client_winch_handle(signo);
}

static volatile int child_pid;
static void (*client_child_handle)(int);

static void sig_child_exit(int signo) 
{ 
    if (client_child_handle)
        client_child_handle(signo);
    if (waitpid(child_pid, NULL, WNOHANG))
        exit(0);
}

void setup_ncurse_signal_handling(void) 
{
    struct sigaction sa, old;
    sa.sa_handler = sig_winch_handle;
    sa.sa_flags   = SA_RESTART; // restart interupted syscalls
    sigemptyset(&sa.sa_mask);   // no need to turn off signals

    if (sigaction(SIGWINCH, &sa, &old) < 0)
        err_sys("sigaction failed in setup signal handling");
    client_winch_handle = old.sa_handler;

    sa.sa_handler = sig_child_exit;
    old.sa_handler = NULL; // just in case.
    if (sigaction(SIGCHLD, &sa, &old) < 0)
        err_sys("sigaction failed in setup signal handling");
    client_child_handle = old.sa_handler;
}

static int* saved_fds = NULL;
static size_t fd_count;

static void save_fds(struct window_placement* interface, size_t count)
{
    size_t idx = 0;
    saved_fds = calloc(count+1, sizeof *saved_fds);

    for_each_win(inter, interface, count) 
        saved_fds[idx++] = dup(inter->fd);

    fd_count = idx;
}    

pid_t handle_ncurses_and_fork(struct window_placement* interface, size_t count)
{
    assert(interface);
    assert(interface[count-1].type == MAIN);

    count--;

    size_t idx = 0;
    int** pipes = NULL;

    if (count) {
        save_fds(interface, count);

        pipes = calloc(count, sizeof *pipes * 2);

        idx = 0;
        for_each_win(var, interface, count) {
            pipes[idx] = calloc(2, sizeof **pipes);
            if (pipe(pipes[idx]) < 0)
                err_sys("pipe failed in ncurses and fork");

            idx++;
        }
        idx = 0;
    }

    int master_fd;
    int stdin_fd = dup(STDIN_FILENO); 

    child_pid = forkpty(&master_fd, NULL, NULL, NULL);
    if (child_pid < 0)
        err_sys("forkpty");

    if (!child_pid) {
        close(master_fd);

        if (dup2(stdin_fd, STDIN_FILENO) < 0)
            err_sys("dup2 failed for the standart input descripor");
        close(stdin_fd);

        if (count) {
            for_each_win(var, interface, count) {
                if (dup2(pipes[idx][1], var->fd) < 0) 
                    err_sys("dup2 failed in child's ncurses and fork");
                close(saved_fds[idx]);

                close(pipes[idx][1]);
                free(pipes[idx]);

                idx++;
            }

            free(saved_fds);
            free(pipes);
        }

        wait_for_signal(SIGUSR2);

        return getppid();
    }
    close(stdin_fd);

    if (count) {
        for_each_win(var, interface, count) 
            var->fd = pipes[idx++][0];

        free(pipes);
    }

    interface[count].fd = master_fd;

    return 0;
}


static void ncurse_loop(struct window_placement* interface, size_t count);

void* setup_client_ncurses(struct window_placement* interface, size_t count)
{
    //pthread_mutex_lock(&setup_lock);
    init_win_array(5);

    init_ncurses();
    setup_ncurse_signal_handling();

    main_win = setup_window(&interface[count-1]);

    for_each_win(var, interface, count) {
        switch (var->type) {
        case MAIN: 
        break;

        case BASIC: 
            setup_window(var);
        break;

        case PANE:
            make_panel(NULL, NULL, var);
        break;

        default: 
            err_info("not recognized window type!");
        }

        if (var->type != FRAME && (var->type >= BASIC && var->type <= PANE)) {
            set_nonblocking(var->fd);
            var->winn->vt = initialize_vterm(var->winn->win);
        }

        if (var->type != MAIN)
            set_nonblocking(saved_fds[var - interface]);
    }
    refresh_windows();

    kill(child_pid, SIGUSR2);
   
    ncurse_loop(interface, count);

    return 0;
}

static void handle_ncurses_resize(void);

static void ncurse_loop(struct window_placement* interface, size_t count)
{
    char buf[1028];
    ssize_t rc;

    int maxy, maxx;

    while (1) 
    { 
        for_each_win(var, interface, count) 
        {
            if ((rc = read(var->fd, buf, sizeof buf)) > 0) {
                vterm_input_write(var->winn->vt, buf, rc);
                render_vterm_diff(var->winn->vt, var->winn->win);
                napms(30);

                if (var->type != MAIN) {
                    if (write(saved_fds[var - interface], buf, rc) < 0)
                        err_sys("write in ncurses loop");
                }
            }

            if (ncurse_resize) {
                getmaxyx(var->winn->win, maxy, maxx);
                vterm_set_size(var->winn->vt, maxy, maxx);
            }
        }

        if (ncurse_resize) {
            ncurse_resize = 0;

            handle_ncurses_resize();
        }
    }
}

void handle_ncurses_resize(void)
{
    endwin();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    struct winsize ws = { 0 };
    ws.ws_row = rows;
    ws.ws_col = cols;

    ioctl(STDIN_FILENO, TIOCSWINSZ, &ws);

//    clear_windows();
    for (int i = 0; i < win_arr_count; i++) 
    {
        if (win_array[i].type == PANE)
            display_panel_border(&win_array[i]);
    }
    refresh_windows();
}

#endif
