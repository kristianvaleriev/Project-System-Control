#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"


int handle_program(int socket, int master_fd, struct client_request* req)
{
    char* program_name = calloc(req->data_size, 1);
    ssize_t rc = recv(socket, program_name, req->data_size, 0);
    if (rc < 0)
        err_sys("recv in program handling");

    if (!rc)
        return errno;
}
