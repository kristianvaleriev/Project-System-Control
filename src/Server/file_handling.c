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
    if (recv_filename(socket, filename, MAX_FILENAME) <= 0) 
        return;

    create_file(socket, filename, req->data_size);
}


static void set_working_dir(char* filename);
static void restore_working_dir(void);

void handle_drivers(int socket, int term_fd, struct client_request* req)
{
    char filename[MAX_FILENAME] = {0};
    if (recv_filename(socket, filename, MAX_FILENAME) <= 0)
        return;

    char* ptr = strrchr(filename, '/');
    if (ptr) {
        size_t len = strlen(++ptr);
        memmove(filename, ptr, len);
        memset(filename + len, 0, MAX_FILENAME - len);
    }

    set_working_dir(filename);

    if (create_file(socket, filename, req->data_size) < 0) {
        info_msg("driver creation failed");
        return;
    }

    restore_working_dir();
}

extern char* program_storage;
static char* saved_cwd;

#define MAX_PATH_ALLOC 256

static void set_working_dir(char* filename)
{
    saved_cwd = calloc(MAX_PATH_ALLOC, 1);
    if (!getcwd(saved_cwd, MAX_PATH_ALLOC)) 
        err_sys("getcwd");

    char* new_wd = make_directory(program_storage, filename, "-mod.d");
    if (chdir(new_wd) < 0) 
        err_sys("chdir on set");

    free(new_wd);
}

static void restore_working_dir(void)
{
    if (saved_cwd) 
        return;

    if (chdir(saved_cwd) < 0)
        err_sys("chdir on restore");

    free(saved_cwd);
}
