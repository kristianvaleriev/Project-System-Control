#include "../../include/includes.h"
#include "../../include/utils.h"

#include <fcntl.h>
#include <ncurses.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <pty.h>

int stdout_fd, stderr_fd;
int is_setup_done = 0; // Probably not needed, but just in case.

pthread_cond_t  setup_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t setup_lock = PTHREAD_MUTEX_INITIALIZER;

void setup_ncurses(void)
{
    pid_t pid = forkpty(&stdout_fd, NULL, NULL, NULL);

    if (!pid) {
        execl("bash", "bash");
        err_info("execl");
        _exit(1);
    }
    FILE* out = fdopen(stdout_fd, "w");

    set_term(newterm(NULL, out, stdin));
    noecho();
    raw();

    int err_fd = open("err.log", O_RDWR | O_CREAT | O_TRUNC, 0755);
    if (err_fd < 0)
        err_info("could not create err.log! Sorry...");

    //int stdout_pipe[2];
    //pipe(stdout_pipe);

    if (dup2(stdout_fd, STDOUT_FILENO) < 0 ||
        dup2(err_fd, STDERR_FILENO) < 0)
        err_sys("dup2 failed in ncurses setup");

    //close(stdout_pipe[1]);

    //stdout_fd = stdout_pipe[0];
    stderr_fd = err_fd;

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


static void ncurse_loop(void);

void* setup_client_ncurses(void* _)
{
    pthread_mutex_lock(&setup_lock);

    setup_ncurses();

    is_setup_done = 1;

    pthread_mutex_unlock(&setup_lock);
    pthread_cond_signal(&setup_cond);

    size_t n_rows = LINES * 0.25;
    WINDOW* frame, *pane;
    make_panel(&frame, &pane, n_rows, COLS, LINES - n_rows, 0);

    wborder(frame, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');

    wrefresh(frame);
    wrefresh(pane);
    refresh();

    ncurse_loop();

    return 0;
}

static void ncurse_loop(void)
{
    char buf[1028];
    ssize_t rc;

    if (fcntl(stderr_fd, F_SETFL, fcntl(stderr_fd, F_GETFL, 0) | O_NONBLOCK)) 
        err_sys("fcntl");

    struct pollfd pfds[] = {
        {
            .fd = stdout_fd,
            .events = POLLIN,
        },
    };

    while (1) 
    {
        while ((rc = poll(pfds, sizeof pfds / sizeof *pfds, 0) <= 0)) {
            if (rc < 0)
                err_sys("poll at server loop failed");
            while ((rc = read(stderr_fd, buf, sizeof buf)) > 0) {
                attron(A_BOLD);
                addnstr(buf, rc);
                attroff(A_BOLD);
                refresh();
            }
        }

        if (pfds[0].revents & POLLIN) 
            while ((rc = read(stdout_fd, buf, sizeof buf)) > 0) {
                addnstr(buf, rc);
                refresh();
            }
    }
}
