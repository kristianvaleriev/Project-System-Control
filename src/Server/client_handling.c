#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <pty.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/wait.h>
#include <errno.h>
#include <poll.h>

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
static int    init_logging_shell(int fd);

static void   main_client_req_loop(int client_socket, 
                                   int master_fd,int pid);
static void   term_reading_function(int,int);
static int    client_req_handle(int,int);


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

    /*
    int rc_err;
    pthread_t reading_thread;
    if ((rc_err = create_reading_thread(reading_thread, reading_function, 
                                        master_fd, client_socket)))
        err_exit(rc_err, "could not create a reading thread");
    */

    main_client_req_loop(client_socket, master_fd, __shell_pid);
    exit(0);
}

static void set_nonblocking(int fd) 
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK)) 
        err_sys("fcntl");
}

static void main_client_req_loop(int client_socket, int master_fd, int pid)
{
    //set_nonblocking(client_socket);
    set_nonblocking(master_fd);

    struct pollfd pfds[] = {
        {
         .fd = client_socket,
         .events = POLLIN,
        },
        {
         .fd = master_fd,
         .events = POLLIN,
        },
    };

    int rc;
    char cmd_buf[128] = {0};
    while (1)
    {
        while ((rc = poll(pfds, sizeof pfds / sizeof *pfds, 0) <= 0)) {
            if (rc < 0)
                err_sys("poll at server loop failed");
        }

        if (pfds[0].revents & POLLIN) {
            if (client_req_handle(client_socket, master_fd) > 1) {
                info_msg("client has left.");
                exit(0);
            }
        }
        else if (pfds[1].revents & POLLIN)             
            term_reading_function(master_fd, client_socket);

        else if (pfds[0].revents & POLLHUP ||
                 pfds[1].revents & POLLHUP) {
            info_msg("client has left. (poll revent is HUP)");
            exit(0);
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

static void term_reading_function(int read_fd, int write_fd)
{
    static char buf[1024];
    ssize_t size;

    if ((size = read(read_fd, buf, sizeof buf)) <= 0) {
        if (!size)
            return;
        if (errno == EIO)
            err_cont(0, "EIO");
        else
            err_info("reading function's read fail");
    }
    buf[size] = '\0';
    //info_msg("sending buf: %s", buf);

    if (send(write_fd, buf, size, MSG_NOSIGNAL) < 0) {
        if (errno == EPIPE)
            info_msg("PIPE"); // <-- & |^|hate this signal. It costed me 3 hours
        else 
            err_info("reading function's write fail");
    }
}

static int client_req_handle(int client_socket, int master_fd)
{
    static char cmd_buf[64];

    struct client_request req;
    ssize_t rc;

    if ((rc = recv(client_socket, &req, sizeof req, 0)) <= 0)  {
        if (!rc)
            return 1;
        err_info("recv of client_req failed");
        return -1;
    }

    if (req.type) 
    {
        req.type = ntohl(req.type);
        req.data_size = ntohl(req.data_size);

        if (!(req.type > 0 && req.type < TYPE_COUNT)) {
            err_cont(0, "detecting not defined reqeust type! "
                     "\"Are you certain whatever you're doing is worth it?\"");
            return -1;
        }

        action_array[req.type](client_socket, master_fd, &req);
    }

    else { // just a bash cmd
        if ((rc = recv(client_socket, cmd_buf, sizeof cmd_buf, 0)) <= 0)  {
            if (!rc)
                return 1;
            err_info("recv of client cmd failed");
            return -1;
        }

        cmd_buf[rc] = '\0';
        write(master_fd, cmd_buf, rc);
    }

    return 0;
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


