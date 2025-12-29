#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <pty.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/poll.h>
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

pid_t __shell_pid = -1;

static void   set_signals(void);
static int    init_logging_shell(int fd);

static void   main_client_req_loop(int client_socket, 
                                   int master_fd,int pid);
static void   term_reading_function(int,int);
static int    client_req_handle(int,int);


static void   exit_handler(int signo)
{
    if (__shell_pid != -1 && waitpid(__shell_pid, NULL, WNOHANG)) 
        _exit(0);
}

void handle_client(int client_socket)
{
    set_signals();

    int master_fd;
    __shell_pid = handle_child_exec(&master_fd, init_logging_shell);

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
    while (1)
    {
        while ((rc = poll(pfds, sizeof pfds / sizeof *pfds, 0) <= 0)) {
            if (rc < 0)
                err_sys("poll at server loop failed");
        }

        if (pfds[0].revents & POLLIN) {
            if ((rc = client_req_handle(client_socket, master_fd)) == CLIENT_QUIT) {
                info_msg("client has left.");
                exit(0);
            }
            else if (rc > 0)
                err_exit(rc, "client request handler reported system error");
            else if (rc < 0)
                err_quit_msg("client request handler reported error id #%d", rc);
        }
        if (pfds[1].revents & POLLIN)             
            term_reading_function(master_fd, client_socket);

        if (pfds[0].revents & POLLHUP ||
            pfds[1].revents & POLLHUP) {
            info_msg("client has left. (poll%d revent is HUP)", 
                     pfds[0].revents & POLLHUP ? 0 : 1);
            exit(0);
        }
        
        // Was wondering why the server takes so long to respond 
        // to user data, then I saw this and cried a bit:
        //    sleep(1); 
    }
}

pid_t handle_child_exec(int* fd, int (*to_exec)(int))
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
        free(slave_name);

        *fd = master_pty;

        return child_pid;
    }
    close(master_pty);

    set_control_terminal(slave_pty, slave_name);

    free(slave_name);
    _exit(to_exec(slave_pty));
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
    ssize_t rc;

    if ((rc = read(read_fd, buf, sizeof buf)) <= 0) {
        if (rc) {
            if (errno == EIO)
                err_cont(0, "EIO");
            else
                err_info("reading function's read fail");
        }
        return;
    }
//    buf[rc] = '\0';
//    info_msg("sending buf: %s", buf);

    if (send(write_fd, buf, rc, MSG_NOSIGNAL) < 0) {
        if (errno == EPIPE)
            info_msg("PIPE"); // <-- & |^|; hate this signal. It costed me 3 hours
        else 
            err_info("reading function's write fail");
    }
}

static int client_req_handle(int client_socket, int master_fd)
{
    static char cmd_buf[64];

    struct client_request req;
    ssize_t rc, size;

    if ((rc = recv(client_socket, &req, sizeof req, 0)) <= 0)  {
        if (!rc)
            return CLIENT_QUIT;
        info_msg("recv of client_req failed");
        return errno;
    }

    if (req.type) 
    {
        req.type = ntohl(req.type);
        req.data_size = ntohl(req.data_size);

        if (!(req.type > 0 && req.type < TYPE_COUNT)) {
            err_cont(0, "detecting not defined reqeust type! "
                     "\"Are you certain whatever you're doing is worth it?\"");
            return 0;
        }

        return action_array[req.type](client_socket, master_fd, &req);
    }

    else { // just a bash cmd
        if ((rc = recv(client_socket, cmd_buf, sizeof cmd_buf, 0)) <= 0)  {
            if (!rc)
                return CLIENT_QUIT;

            info_msg("recv of client cmd failed");
            return errno;
        }

        size = 0;
        for (ssize_t i = 0; i < rc; i++) {
            // somehow those weren't the same STUPID thing on rpi4. HOW?!
            if (cmd_buf[i] != 0 /*|| cmd_buf[i] == '\0'*/) 
                cmd_buf[size++] = cmd_buf[i];
            else if (cmd_buf[i+1] == 0 && cmd_buf[i+7] == 0)
                i += 8;
        }

        write(master_fd, cmd_buf, size);
    }

    return 0;
}


static void set_signals(void)
{
    struct sigaction sig_chld;
    sig_chld.sa_handler = SIG_IGN;
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


