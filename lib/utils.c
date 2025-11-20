#include <string.h>


char* set_program_name(char const* argv)
{
    // Extract program's name from argv[0] 
    char* ptr = strrchr(argv, '/');
    size_t offset = 0;
    if (ptr) {
        offset = ptr - argv;
    } 

  extern char* program_name;
    program_name = strdup(argv + offset);

    return program_name;
}

