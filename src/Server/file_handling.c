#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <sys/stat.h>


#define ACCESS_MODE 0744

static int create_file_path(char* pathname)
{
    char* buf = calloc(MAX_FILENAME, 1);
    size_t len = strlen(pathname);
    size_t offset = 0;

    for (size_t i = 0; i < len; i++) 
    {
        if (pathname[i] != '/')
            continue;

        memcpy(buf + offset, pathname + offset, i - offset);
        offset = i;

        mkdir(buf, ACCESS_MODE);
    }

    free(buf);
    return 0;
}

static void write_file(int socket, int fd, size_t size)
{
    size_t rc;
    char* buf = calloc(size, 1);

    while ((rc = recv(socket, buf, size, 0)) > 0) {
        if (write(fd, buf, rc) < 0) {
            err_info("write fail");
            return;
        }

        if (rc >= size)
            break;
    }
    free(buf);

    if (rc < 0)
        err_info("recv() in write to file");
}

void handle_files(int socket, int term_fd, struct client_request* req)
{
    char filename[MAX_FILENAME] = {0};
    if (recv(socket, filename, MAX_FILENAME, 0) <= 0)
        return;

    if (strstr(filename, "/"))
        if (create_file_path(filename))
            return;

    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, ACCESS_MODE);
    if (fd < 0) {
        err_info("could not create file '%s'", filename);
        return;
    }

    write_file(socket, fd, req->data_size);

    close(fd);

    info_msg("file '%s' successfully stored", filename);
}

