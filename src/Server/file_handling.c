#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

void handle_files(int socket, int term_fd, struct client_request* req)
{
    char filename[MAX_FILENAME] = {0};
    if (recv(socket, filename, MAX_FILENAME, 0) <= 0)
        return;

    int fd = open(filename, O_RDWR | O_CREAT);
    if (fd < 0) {
        err_info("could not create file '%s'", filename);
        return;
    }

    size_t rc;
    char buf[4098] = {0};
    while ((rc = recv(socket, buf, req->data_size, 0)))
        write(fd, buf, rc);
}

