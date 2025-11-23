#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <pty.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "networking.h"
#include "server_pty.h"

static char* shell_argv[] = {
    "/bin/bash",
    "--login",
    NULL
};

static int    handle_child_exec(int* fd);
static int    init_logging_shell(int fd);
static void*  shell_reading_thread(void*);

void handle_client(int client_socket)
{
    int master_fd;
    int shell_pid = handle_child_exec(&master_fd);

    pthread_t reading_thread;
    pthread_create(&reading_thread, NULL, shell_reading_thread, 
                   (int[]) {master_fd, client_socket});

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

        info_msg("data recieved: %s", buf);

        write(master_fd, buf, rc);

        sleep(1);
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

void* shell_reading_thread(void* fd)
{
    int* master_fd = (int*) fd; 
    int* client_socket = (int*) (fd + sizeof(int));

    ssize_t size;
    char buf[4098];
    while (1)
    {
        if ((size = read(*master_fd, buf, sizeof buf)) < 0)
            err_sys("read");

        buf[size + 1] = '\0';
        info_msg("sending buf: %s", buf);
   
        if (sendall(*client_socket, buf, size, 0) < 0)
            err_sys("sendall");
    }

    return 0;
}
