#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <pty.h>
#include <pthread.h>
#include <signal.h>

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

static void   sigchld_handler(int signo)
{
    info_msg("client has left");
    exit(0);
}

void handle_client(int client_socket)
{
    set_signals();

    int master_fd;
    int shell_pid = handle_child_exec(&master_fd);

    pthread_t reading_thread;
    create_reading_thread(reading_thread, master_fd, client_socket);

    int rc;
    while (1)
    {
        char buf[128] = {0};
        if ((rc = recv(client_socket, buf, sizeof buf, 0)) < 0)
            err_info("recv failed");
        else if (!rc) {
            info_msg("client closed the connection");
            break;
        }

        write(master_fd, buf, rc);

        // Was wondering why the server takes so long to respond 
        // to user data and then I saw this:
        //  sleep(1); 
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
    sig_chld.sa_handler = sigchld_handler;
    sig_chld.sa_flags   = SA_NOCLDSTOP; // restart interupted syscalls
    sigemptyset(&sig_chld.sa_mask);   // no need to turn off signals
    if (sigaction(SIGCHLD, &sig_chld, NULL) < 0)
        err_sys("sigaction failed");
}
