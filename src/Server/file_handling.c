#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <sys/stat.h>
#include <assert.h>

#define ACCESS_MODE   0744
#define DRIVER_DIR    "Drivers.d/"


static void flush_recv_out(int socket, size_t size)
{
    char* buf = malloc(size);
    size_t rc;
    while ((rc = recv(socket, buf, size, 0)) > 0) {
        if (rc >= size)
            break;
        size -= rc;
    }

    free(buf);
}

static void create_file_path(char* pathname)
{
    char* buf = calloc(MAX_FILENAME, 1);
    if (!buf)
        return; 

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
    return;
}

static void write_file(int socket, int fd, size_t size)
{
    size_t rc;
    char* buf = calloc(size, 1);

    while ((rc = recv(socket, buf, size, 0)) > 0) {
        if (write(fd, buf, rc) < 0) 
            err_info("write fail");
        
        if (rc >= size)
            break;
        size -= rc;
    }
    free(buf);

    if (rc < 0)
        err_info("recv() in write to file");
}

static int create_file(int socket, char* filename, size_t filesize)
{
    if (strstr(filename, "/"))
        create_file_path(filename);

    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, ACCESS_MODE);
    if (fd < 0) {
        err_info("could not create file '%s'", filename);
        flush_recv_out(socket, filesize);
        return -1;
    }

    write_file(socket, fd, filesize);

    close(fd);

    info_msg("file '%s' successfully stored", filename);
    return 0;
}

static ssize_t recv_filename(int socket, char* filename, size_t size)
{
    ssize_t rc = recv(socket, filename, MAX_FILENAME, 0);
    if (rc < 0) {
        err_info("recv on filename");
        flush_recv_out(socket, size);
    }

    return rc;
}

void handle_files(int socket, int term_fd, struct client_request* req)
{
    char filename[MAX_FILENAME] = {0};
    if (recv_filename(socket, filename, req->data_size) <= 0) 
        return;

    create_file(socket, filename, req->data_size);
}

void handle_drivers(int socket, int term_fd, struct client_request* req)
{
    char filename[MAX_FILENAME] = {0};
    if (recv_filename(socket, filename, req->data_size) <= 0)
        return;

    char* ptr = strrchr(filename, '/');
    if (ptr) {
        size_t len = strlen(++ptr);
        memmove(filename, ptr, len);
        filename[len] = '\0';
    }

    char path_buf[MAX_FILENAME + sizeof DRIVER_DIR] = {0};
    snprintf(path_buf, sizeof path_buf, "%s%s", DRIVER_DIR, filename);

    if (create_file(socket, path_buf, req->data_size) < 0) {
        info_msg("driver creation failed");
        return;
    }
}

extern char* program_storage;

static void set_working_dir(char* filename)
{
    assert(filename != NULL);

    size_t size = strlen(program_storage) + MAX_FILENAME + 1;
    char* new_cwd = calloc(size, 1);
    snprintf(new_cwd, size, "%s/%s.d", program_storage, filename);
}
