#ifndef WITHOUT_NCURSES

#include "../../include/includes.h"
#include "../../include/utils.h"

#include <sys/wait.h>
#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <vterm.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <pty.h>

#include "client_ncurses.h"

#define LOG_NAME "err.log"

/*
int is_setup_done = 0; // Probably not needed, but just in case.

pthread_cond_t  setup_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t setup_lock = PTHREAD_MUTEX_INITIALIZER;
*/ 

static WINDOW* pane;

struct win_array {
    WINDOW** array;
    size_t count;
    size_t alloced;
} windows;

static void init_win_array(int n)
{
    windows.array = calloc(n, sizeof(WINDOW*));
    windows.count = 0;
    windows.alloced = n;
}

static void insert_into_windows(WINDOW* win)
{
    size_t idx = windows.count++;
    if (idx < windows.alloced)
        windows.array[idx] = win;
}

static void refresh_windows(void)
{
    for (size_t i = 0; i < windows.count; i++)
        wrefresh(windows.array[i]);
}


void setup_ncurses(void)
{
    initscr();
    noecho();
    raw();

    start_color();
    init_pair(1, COLOR_BLUE, COLOR_BLACK);

    scrollok(stdscr, TRUE);

    atexit((void(*)(void)) endwin);
   
    /*
    if (//dup2(slave_fd, STDIN_FILENO)  < 0 ||
        dup2(pipe_fds[1], STDOUT_FILENO) < 0 ||
        dup2(err_fd,   STDERR_FILENO) < 0)
        err_sys("dup2 failed in ncurses setup");
    */

    //close(slave_fd);
    //close(pipe_fds[1]);

    //stdout_fd = pipe_fds[0];
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


static int stdin_fd;
static int stdout_fd;
static int child_pid;

static void ncurse_loop(void);
static void sig_child_exit(int _) 
{ 
    int status;
    if (waitpid(child_pid, &status, WNOHANG))
        if (WIFEXITED(status))
            exit(0);
}

void* setup_client_ncurses(void* _)
{
    int input_pipe[2]; pipe(input_pipe);
    int saved_stdin = dup(STDOUT_FILENO);

    child_pid = forkpty(&stdout_fd, NULL, NULL, NULL);
    if (child_pid < 0)
        err_sys("forkpty");


    if (!child_pid) {
        close(input_pipe[1]);

        int err_fd = open(LOG_NAME, O_RDWR | O_CREAT | O_TRUNC, 0755);
        if (err_fd < 0)
            err_info("could not create err.log! Sorry...");

        dup2(saved_stdin, STDIN_FILENO);
        dup2(err_fd, STDERR_FILENO);
        close(err_fd);

        sigset_t sigset, oldmask;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR2);
        sigprocmask(SIG_SETMASK, &sigset, &oldmask);

        int signo = -1;
        while(signo != SIGUSR2) 
            sigwait(&sigset, &signo); 

        sigprocmask(SIG_SETMASK, &oldmask, NULL);

        return 0;
    }

    close(input_pipe[0]);
    stdin_fd = input_pipe[1];
    if (signal(SIGCHLD, sig_child_exit) < 0)
        err_sys("signal in setup client ncurses");

    //pthread_mutex_lock(&setup_lock);

    setup_ncurses();
    setup_vterm();
    init_win_array(5);

    //is_setup_done = 1;

    //pthread_mutex_unlock(&setup_lock);
    //pthread_cond_signal(&setup_cond);

    size_t n_rows = LINES * 0.25;
    WINDOW* frame;
    make_panel(&frame, &pane, n_rows, COLS, LINES - n_rows, 0);

    wborder(frame, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');

    insert_into_windows(stdscr);
    insert_into_windows(frame);
    insert_into_windows(pane);
    refresh_windows();

    kill(child_pid, SIGUSR2);
   
    ncurse_loop();

    return 0;
}

static void ncurse_loop(void)
{
    char buf[1028];
    ssize_t rc;

    int stderr_fd = open(LOG_NAME, O_RDONLY);
    set_nonblocking(stderr_fd);
    set_nonblocking(stdout_fd);

    struct pollfd pfds[] = {
        {
            .fd = stdout_fd,
            .events = POLLIN,
        },
    };

    while (1) 
    {
        while ((rc = poll(pfds, sizeof pfds / sizeof *pfds, 0)) <= 0) {
            if (rc < 0)
                err_sys("poll at server loop failed");

            while ((rc = read(stderr_fd, buf, sizeof buf)) > 0) {
                wattron(pane, A_BOLD);
                waddnstr(pane, buf, rc);
                wattroff(pane, A_BOLD);

                refresh_windows();
            }
        }

        if (pfds[0].revents & POLLIN) {
            if ((rc = read(stdout_fd, buf, sizeof buf)) > 0) 
                vterm_write_to_input(buf, rc);

            //vterm_screen_flush_damage(vts);
            render_vterm_diff();
        }

        //char ch;
        //read(STDIN_FILENO, &ch, 1 );
        //write(stdin_fd, &ch, 1);
    }
}

#endif
