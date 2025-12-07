#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <asm-generic/errno-base.h>
#include <pty.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/wait.h>
#include <errno.h>

#include "networking.h"
#include "server_pty.h"
#include "action_array.h"

static char* shell_argv[] = {
    "/bin/bash",
    "--login",
    NULL
};

static pid_t __shell_pid = -1;

static void   set_signals(void);
static pid_t  handle_child_exec(int* fd);
static void*  reading_function(void*);
static int    init_logging_shell(int fd);
static void   main_client_req_loop(int client_socket, 
                                   int master_fd,int pid);

static void   exit_handler(int signo)
{
    if (waitpid(__shell_pid, NULL, WNOHANG)) 
        _exit(0);
}

void handle_client(int client_socket)
{
    set_signals();

    int master_fd;
    __shell_pid = handle_child_exec(&master_fd);

    int rc_err;
    pthread_t reading_thread;
    if ((rc_err = create_reading_thread(reading_thread, reading_function, master_fd, client_socket)))
        err_exit(rc_err, "could not create a reading thread");

    main_client_req_loop(client_socket, master_fd, __shell_pid);
    exit(0);
}


static int  recv_wrapper(int, void*, size_t);

static void main_client_req_loop(int client_socket, int master_fd, int pid)
{
    int rc;
    struct client_request req = {0};

    char cmd_buf[128] = {0};
    while (1)
    {
        if ((rc = recv(client_socket, &req, sizeof req, 0)) < 0) 
            continue;
        if (!rc)
            exit(0);

        if (req.type) 
        {
            req.type = ntohl(req.type);
            req.data_size = ntohl(req.data_size);

            if (!(req.type > 0 || req.type < TYPE_COUNT)) {
                err_cont(0, "detecting not defined reqeust type! "
                         "\"Are you certain whatever you're doing is worth it?\"");
                continue;
            }

            action_array[req.type](client_socket, master_fd, &req);

            memset(&req, 0, sizeof req);
        }
        else { // just a bash cmd
            if ((rc = recv(client_socket, cmd_buf, sizeof cmd_buf, 0)) < 0) 
                continue;
            if(!rc)
                exit(0);

            write(master_fd, cmd_buf, rc);
        }
        
        // Was wondering why the server takes so long to respond 
        // to user data, then I saw this and cried a bit:
        //    sleep(1); 
    }
}

static pid_t handle_child_exec(int* fd)
{
    int master_pty, slave_pty;

    char* slave_name = malloc(128);
    if (openpty(&master_pty, &slave_pty, slave_name, NULL, NULL)) 
        err_sys("openpty failed");

    pid_t child_pid = fork();
    if (child_pid < 0)
        err_sys("fork error");
    
    //Parent returns here ::
    if (child_pid) {
        close(slave_pty);
        *fd = master_pty;

        return child_pid;
    }
    close(master_pty);

    set_control_terminal(slave_pty, slave_name);

    free(slave_name);
    _exit(init_logging_shell(slave_pty));
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
    err_info("execv failed"); 

    return -5;
}

static void set_signals(void)
{
    struct sigaction sig_chld;
    sig_chld.sa_handler = exit_handler;
    sig_chld.sa_flags = SA_RESTART;
    sigemptyset(&sig_chld.sa_mask);   
    
    if (sigaction(SIGCHLD, &sig_chld, NULL) < 0)
        err_sys("sigaction failed");
}

static int recv_wrapper(int socket, void* buf, size_t size)
{
    int ret = recv(socket, buf, size, 0);
    if (ret < 0) {
        err_info("recv failed");
    }
    else if (ret == 0) 
        exit(0);

    return ret;
}

static void* reading_function(void* fds)
{
    int read_fd  = *((int*) fds); 
    int write_fd = *((int*) (fds + sizeof(int)));

    ssize_t size;
    char buf[2049];
    while (1)
    {
        if ((size = read(read_fd, buf, sizeof buf)) < 0) {
            if (errno == EIO)
                err_cont(0, "EIO");
            else
                err_info("reading function's read fail");
            break;
        }
        buf[size] = '\0';
        //info_msg("sending buf: %s", buf);

        if (send(write_fd, buf, size, MSG_NOSIGNAL) < 0) {
            if (errno == EPIPE)
                info_msg("PIPE"); // <-- & |^|hate this signal. It costed me 3 hours
            else 
                err_info("reading function's write fail");
            break;
        }
    }

    return 0;
}


