#include "../include/includes.h"
#include "../include/utils.h"

#include <asm-generic/errno-base.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>

extern char* program_name;

char* set_program_name(char const* argv)
{
    // Extract program's name from argv[0] 
    char* ptr = strrchr(argv, '/');
    size_t offset = 0;
    if (ptr) {
        offset = ptr - argv + 1;
    } 

    program_name = strdup(argv + offset);

    return program_name;
}

void set_program_storage(const char* prefix)
{
    assert(prefix != NULL);

  extern char* program_storage;

    program_storage = make_directory(prefix, program_name, ".d");
    if (!program_storage)
        err_sys("could not create storage directory");

    info_msg("programe storage '%s' created", program_storage);
}

char* make_directory(const char* prefix, const char* name, const char* suffix)
{
    assert(prefix != NULL && name != NULL && suffix != NULL);

    size_t size = strlen(prefix) + strlen(name) + strlen(suffix) + 1;
    char* ret = calloc(size, 1);
    if (!ret)
        return NULL;

    snprintf(ret, size, "%s/%s%s", prefix, name, suffix);

    if (mkdir(ret, 0744) < 0)
        if (errno != EEXIST)
            return NULL;

    return ret;
}

void* reading_function(void* fds)
{
    int* read_fd  = (int*) fds; 
    int* write_fd = (int*) (fds + sizeof(int));

    ssize_t size;
    char buf[2049];
    while (1)
    {
        if ((size = read(*read_fd, buf, sizeof buf)) < 0) {
            if (errno == EIO)
                err_cont(0, "EIO");
            else
                err_info("reading function's read fail");
            break;
        }
        buf[size] = '\0';
        //info_msg("sending buf: %s", buf);

        if (write(*write_fd, buf, size) < 0) {
            err_info("reading function's write fail");
            break;
        }
    }

    return 0;
}


