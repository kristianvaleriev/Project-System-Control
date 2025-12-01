#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "dynamic_files.h"
#include "client_networking.h"

#define MAX 5

struct filename_array* init_filename_array(void)
{
    struct filename_array* ret = calloc(1, sizeof *ret);
    if (ret) {
        ret->filenames = calloc(MAX, sizeof(void*));
        ret->max_alloced = MAX;
        ret->count = 0;
    }

    return ret;
}

void insert_in_array(struct filename_array* arr, char* filename, size_t size)
{
    if (!arr || !arr->filenames)
        return;

    size_t* idx = &arr->count;
    arr->filenames[(*idx)++] = strndup(filename, size);
    
    if (*idx == arr->max_alloced) {
        arr->max_alloced += MAX;
        arr->filenames = realloc(arr->filenames, arr->max_alloced);
    }
}

void dealloc_filename_array(struct filename_array* ptr)
{
    if (!ptr) return;

    if (ptr->filenames) {
        for (size_t i = 0; i < ptr->count; i++)
            free(ptr->filenames[i]);
        free(ptr->filenames);
    }

    free(ptr);
}

static void send_file_data(int socket, char* filename)
{
    char fname[MAX_FILENAME] = {0};
    memcpy(fname, filename, MAX_FILENAME);

    int fd = open(fname, O_RDONLY);
    if (fd < 0) {
        err_info("could not open() file '%s'", fname);
        return;
    }

    if (sendall(socket, fname, MAX_FILENAME, 0)) {
        err_info("could not send file name");
        return;
    }
    
    int rc;
    char file_data[4096] = {0};
    while ((rc = read(fd, file_data, sizeof file_data)) > 0)
    {
        if (sendall(socket, file_data, rc, 0) < 0) {
            err_info("sendall of file contents failed");
            break;
        }
    }
}

static int send_file_req(int socket, int type, char* filename)
{
    struct stat file_stats = {0};
    if (stat(filename, &file_stats) < 0) {
        err_info("stat function of file '%s' failed", filename);
        return -1;
    }

    struct client_request req = {
        .type = htonl(type),
        .data_size = htonl(file_stats.st_size),
    };

    if (sendall(socket, &req, sizeof req, 0) < 0) {
        err_info("sendall of file request failed");
        return -2;
    }

    return 0;
}

void handle_file_send(int socket, int type, struct filename_array* arr)
{
    for (size_t i = 0; i < arr->count; i++)
    {
        if (!send_file_req(socket, type, arr->filenames[i]))
             send_file_data(socket, arr->filenames[i]);
        info_msg("file: %s", arr->filenames[i]);
    }
}

int fork_handle_file_send(int type, struct filename_array* arr)
{
    if (!arr->count)
        return 0;

    int pid = fork();
    if (pid < 0) {
        err_info("fork() in file send");
        return -2;
    }
    if (pid) return pid;

    int socket = new_connected_server_socket();

    handle_file_send(socket, type, arr);

    exit(0);
}
