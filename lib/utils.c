#include "../include/includes.h"
#include "../include/utils.h"

#include <asm-generic/errno-base.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/socket.h>

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

    info_msg("using program storage '%s'", program_storage);
}

char* make_directory(const char* prefix, const char* name, const char* suffix)
{
    assert(prefix != NULL && name != NULL && suffix != NULL);

    size_t size = strlen(prefix) + strlen(name) + strlen(suffix) + 2;
    char* ret = calloc(size, 1);
    if (!ret)
        return NULL;

    snprintf(ret, size, "%s/%s%s", prefix, name, suffix);

    if (mkdir(ret, 0744) < 0)
        if (errno != EEXIST)
            return NULL;

    return ret;
}

