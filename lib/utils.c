#include "../include/includes.h"
#include "../include/utils.h"

#include <pthread.h>

char* set_program_name(char const* argv)
{
    // Extract program's name from argv[0] 
    char* ptr = strrchr(argv, '/');
    size_t offset = 0;
    if (ptr) {
        offset = ptr - argv + 1;
    } 

  extern char* program_name;
    program_name = strdup(argv + offset);

    return program_name;
}

void* reading_function(void* fds)
{
    int* read_fd  = (int*) fds; 
    int* write_fd = (int*) (fds + sizeof(int));

    ssize_t size;
    char buf[4098];
    while (1)
    {
        if ((size = read(*read_fd, buf, sizeof buf)) < 0)
            err_sys("reading function's read fail");
        buf[size] = '\0';
        //info_msg("sending buf: %s", buf);
   
        if (write(*write_fd, buf, size) < 0)
            err_sys("reading function's write fail");
    }

    return 0;
}
