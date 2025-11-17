#include "../../include/includes.h"
#include "../../include/error_handling.h"

#include "tty_conf.c"
#include <unistd.h>

void get_program_name(char const* argv);

char* program_name = 0;

int main(int argc, char** argv)
{
    get_program_name(argv[0]);

    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw mode");

    // Very funny if that here is missing
    // (also bad tho :\)
    if (atexit(set_tty_atexit)) {
        perror("atexit");
        exit(1);
    }

    for (int i = 0; i < 4; i++)
    {
        char buf[128] = { 0 };

        read(STDIN_FILENO, buf, 2);
        printf("char: %c,%d %c,%d\n\r", buf[0], buf[0], buf[1], buf[1]);

        sleep(1);
    }
}

void get_program_name(char const* argv)
{
    // Extract program's name from argv[0] 
    char* ptr = strrchr(argv, '/');
    size_t offset = 0;
    if (ptr) {
        offset = ptr - argv;
    } 
    program_name = strdup(argv + offset);
}

