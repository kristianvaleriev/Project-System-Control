#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <signal.h>
#include <unistd.h>

#include "action_array.h"

#define DELIM " \t\n"

extern pid_t __shell_pid;

static char* client_program_name = NULL;

int exec_dedicated_prog(int fd)
{
    //int cpy = dup(STDERR_FILENO);

    if (dup2(fd, STDOUT_FILENO) < 0 ||
        dup2(fd, STDERR_FILENO) < 0 ||
        dup2(fd, STDIN_FILENO)  < 0) 
        err_sys("dup2 in program handling");

    if (fd != STDIN_FILENO || fd != STDOUT_FILENO ||
        fd != STDERR_FILENO)
        close(fd);

    size_t idx = 0;
    char* buf[32] = { 0 };
    char* tok = strtok(client_program_name, DELIM);

    if (tok) {
        do {
            buf[idx++] = tok;
        } while((tok = strtok(NULL, DELIM)) && idx < (sizeof buf));
    } 
    else 
        buf[0] = client_program_name;

    execvp(client_program_name, buf);

    //dup2(cpy, STDERR_FILENO);
    //err_info("execvp in program handling (program name: %s)", client_program_name);

    return errno;
}

int handle_program(int socket, int master_fd, struct client_request* req)
{
    client_program_name = calloc(req->data_size, 1);
    ssize_t rc = recv(socket, client_program_name, req->data_size, 0);
    if (rc < 0)
        err_sys("recv in program handling");

    if (!rc)
        return errno;

    info_msg("trying to start dedicated client program '%s'", client_program_name);

    kill(SIGINT, __shell_pid);

    int new_master_fd;
    __shell_pid = handle_child_exec(&new_master_fd, exec_dedicated_prog);

    if (dup2(new_master_fd, master_fd) < 0)
        err_sys("dup2 in program handling");

    return 0;
}
