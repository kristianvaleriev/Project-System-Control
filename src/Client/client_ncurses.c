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
static WINDOW* main_pane;

typedef struct {
    WINDOW* frame;
    chtype border_chs[8];
} win_pane_t;

struct window_node {
    WINDOW* win;
    
    int type;
    void* data;
}* win_array;

static size_t win_arr_count;
static size_t win_arr_alloced;

static void init_win_array(int n)
{
    win_array = calloc(n, sizeof *win_array);
    for (int i = 0; i < n; i++) 
    {
        win_array[i].win  = NULL;
        win_array[i].data = NULL;
        win_array[i].type = BASIC;
    }

    win_arr_count = 0;
    win_arr_alloced = n;
}

static size_t insert_into_windows(WINDOW* win, int type, void* data)
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

    return idx;
}
#define insert_into_windows_def(win) \
        insert_into_windows(win, 0, NULL)

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


void init_ncurses(WINDOW** std_win)
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
   
    insert_into_windows_def(stdscr);

    if (!std_win) return;

    WINDOW* win = newwin(LINES, COLS, 0, 0);
    insert_into_windows_def(win);

    wattrset(win, A_NORMAL);
    idlok(win, FALSE);
    scrollok(win, FALSE);
    leaveok(win, TRUE);

    *std_win = win;
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

void make_panel(WINDOW** frame, WINDOW** pane, chtype* border,
                int rows, int cols, int starty, int startx)
{
    *frame = newwin(rows, cols, starty, startx);
    *pane  = derwin(*frame, rows - 1, cols - 1, begin_row, begin_col);

    if (!*frame || !*pane) {
        err_cont(0, "window creating failed");
        return;
    }

    int maxy, maxx;
    if (main_win) {
        getmaxyx(main_win, maxy, maxx);
        wresize(main_win, maxy - rows, maxx);
    }
    else {
        getmaxyx(stdscr, maxy, maxx);
        wresize(stdscr, maxy - rows, maxx);
    }

    win_pane_t* new_pane = malloc(sizeof *new_pane);
    new_pane->frame = *frame;
    memcpy(new_pane->border_chs, border, 8 * sizeof *border);

    size_t idx;
    insert_into_windows(*frame, FRAME, &win_array[win_arr_count]);
    idx = insert_into_windows(*pane, PANE, new_pane);

    display_panel_border(&win_array[idx]);
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


static int stdout_fd;
static void ncurse_loop(void);

static int* saved_fds = NULL;

static void save_fds(struct window_placement* interface, size_t count)
{
    size_t idx = 0;
    saved_fds = calloc(count+1, sizeof *saved_fds);
    saved_fds[idx++] = dup(STDIN_FILENO);

    for_each_win(inter, interface, count) {
        if (inter->type == MAIN)
            continue;
        saved_fds[idx++] = dup(inter->fd);
    }
}    

static void restore_fds(struct window_placement* interface, size_t count)
{
    size_t idx = 0;
    if (dup2(saved_fds[idx], STDIN_FILENO) < 0)
        err_sys("dup2 failed when restoring STDIN");

    close(saved_fds[idx++]);

    for_each_win(var, interface, count) {
        if(var->type == MAIN)
            continue;
    
        if (dup2(saved_fds[idx], var->fd) < 0)
            err_sys("dup2 failed when restoring fds");

        close(saved_fds[idx++]);
    }

    free(saved_fds);
}

pid_t handle_ncurses_and_fork(struct window_placement* interface, size_t count)
{
    save_fds(interface, count);

    child_pid = forkpty(&stdout_fd, NULL, NULL, NULL);
    if (child_pid < 0)
        err_sys("forkpty");

    if (!child_pid) {
        restore_fds(interface, count);

        sigset_t sigset, oldmask;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR2);
        sigprocmask(SIG_SETMASK, &sigset, &oldmask);

        int signo = -1;
        while(signo != SIGUSR2) 
            sigwait(&sigset, &signo); 

        sigprocmask(SIG_SETMASK, &oldmask, NULL);

        return getppid();
    }
   
    return 0;
}

void* setup_client_ncurses(struct window_placement* interface, size_t count)
{
    //pthread_mutex_lock(&setup_lock);
    init_win_array(5);

    init_ncurses(&main_win);
    setup_ncurse_signal_handling();

    //is_setup_done = 1;

    //pthread_mutex_unlock(&setup_lock);
    //pthread_cond_signal(&setup_cond);

    chtype border_chs[8]; 
    memset(border_chs, ' ', 8 * sizeof *border_chs);
    border_chs[2] = 0;

    int y,x;
    getmaxyx(main_win, y, x);
    size_t n_rows = y * 0.25;

    WINDOW* frame;
    make_panel(&frame, &main_pane, border_chs, n_rows, x, y - n_rows, 0);
    refresh_windows();

    kill(child_pid, SIGUSR2);
   
    ncurse_loop();

    return 0;
}

static void handle_ncurses_resize(void);

static void ncurse_loop(void)
{
    char buf[1028];
    ssize_t rc;

    void* out_vt = initialize_vterm(main_win);
    void* err_vt = initialize_vterm(main_pane);

    int stderr_fd = open(LOG_NAME, O_RDONLY);
    set_nonblocking(stderr_fd);
    set_nonblocking(stdout_fd);

    while (1) 
    { 
        if (ncurse_resize) {
            ncurse_resize = 0;
            handle_ncurses_resize();
        }

        if ((rc = read(stderr_fd, buf, sizeof buf)) > 0) {
            vterm_input_write(err_vt, buf, rc);
            render_vterm_diff(err_vt, main_pane);
            napms(80);
        }

        while ((rc = read(stdout_fd, buf, sizeof buf)) > 0) {
            vterm_input_write(out_vt, buf, rc);
            render_vterm_diff(out_vt, main_win);
        }

    }
}

void handle_ncurses_resize(void)
{
    endwin();
    clear_windows();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /*
    extern VTerm* vt;
    vterm_set_size(vt, rows, cols);
    */

    struct winsize ws = { 0 };
    ws.ws_row = rows;
    ws.ws_col = cols;

    ioctl(stdout_fd, TIOCSWINSZ, &ws);

    clear_windows();
    for (int i = 0; i < win_arr_count; i++) 
    {
        if (win_array[i].type == PANE)
            display_panel_border(&win_array[i]);
    }
    refresh_windows();
}

#endif
