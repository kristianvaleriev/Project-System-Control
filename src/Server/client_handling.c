#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <pty.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>

#include "networking.h"
#include "server_pty.h"

static char* shell_argv[] = {
    "/bin/bash",
    "--login",
    NULL
};

static void   set_signals(void);
static int    handle_child_exec(int* fd);
static int    init_logging_shell(int fd);
static void   main_client_req_loop(int client_socket, 
                                   int master_fd,int pid);

static void   exit_handler(int signo)
{
    info_msg("client has left");
    exit(0);
}

void handle_client(int client_socket)
{
    set_signals();

    int master_fd;
    int shell_pid = handle_child_exec(&master_fd);

    int rc_err;
    pthread_t reading_thread;
    if ((rc_err = create_reading_thread(reading_thread, master_fd, client_socket)))
        err_exit(rc_err, "could not create a reading thread");

    main_client_req_loop(client_socket, master_fd, shell_pid);
    exit_handler(0);
}


static int  recv_wrapper(int, void*, size_t);
static void set_win_size(int client_socket, size_t size, int fd);

static void main_client_req_loop(int client_socket, int master_fd, int pid)
{
    int rc;
    struct client_request req = {0};

    while (1)
    {
        if ((rc = recv_wrapper(client_socket, &req, sizeof req)) < 0) 
            continue;

        if (req.type) 
        {
            req.data_size = ntohl(req.data_size);

            switch (ntohl(req.type)) {
            case TYPE_WINSIZE:
                set_win_size(client_socket, req.data_size, master_fd);
                break;

            case TYPE_DRIVERS:
                break;

            case TYPE_FILES:
                break;

            default: 
                err_cont(0, "detecting not defined reqeust type! "
                         "\"Are you certain whatever you're doing is worth it?\"");
            }
        }
        else {
            char buf[128] = {0};
            if ((rc = recv_wrapper(client_socket, buf, sizeof buf)) < 0) 
                continue;

            write(master_fd, buf, rc);
        }
        
        // Was wondering why the server takes so long to respond 
        // to user data, then I saw this and cried a bit:
        //    sleep(1); 
    }
}

static int handle_child_exec(int* fd)
{
    int master_pty, slave_pty;
    char* slave_name = malloc(128);
    if (openpty(&master_pty, &slave_pty, slave_name, NULL, NULL)) 
        err_sys("openpty failed");

    pid_t child_pid = fork();
    if (child_pid < 0)
        err_sys("fork error");
    
    /*
    * Parent returns here ::
    */
    if (child_pid) {
        close(slave_pty);
        *fd = master_pty;

        return child_pid;
    }
    close(master_pty);

    set_control_terminal(slave_pty, slave_name);

    free(slave_name);
    exit(init_logging_shell(slave_pty));
}

static int init_logging_shell(int fd)
{
    
    if (dup2(fd, STDIN_FILENO)  != STDIN_FILENO)
        return -2;
    if (dup2(fd, STDOUT_FILENO) != STDOUT_FILENO)
        return -3;
    if (dup2(fd, STDERR_FILENO) != STDERR_FILENO)
        return -4;

    if (fd != STDIN_FILENO || fd != STDOUT_FILENO ||
        fd != STDERR_FILENO)
        close(fd);

    execv(shell_argv[0], &shell_argv[0]);
    err_sys("execv failed"); // shouldn't happen
    
    return -5;
}

void set_signals(void)
{
    struct sigaction sig_chld;
    sig_chld.sa_handler = exit_handler;
    sig_chld.sa_flags   = SA_NOCLDSTOP; // restart interupted syscalls
    sigemptyset(&sig_chld.sa_mask);   // no need to turn off signals
    
    if (sigaction(SIGCHLD, &sig_chld, NULL) < 0)
        err_sys("sigaction failed");
}

static int recv_wrapper(int socket, void* buf, size_t size)
{
    int ret = recv(socket, buf, size, 0);
    if (ret < 0) {
        err_info("recv failed");
    }
    else if (ret == 0) {
        info_msg("client has closed the connection");
        exit_handler(0);
    } 

    return ret;
}

static void set_win_size(int client_socket, size_t size, int fd)
{
    struct winsize wins;
    if (recv_wrapper(client_socket, &wins, size) < 0) {
        info_msg("could not retrive winsize struct");
        return;
    }

    if (ioctl(fd, TIOCSWINSZ, &wins) < 0)
        err_sys("ioctl");

    info_msg("terminal window size is changed");
}
