#include "../../include/includes.h"
#include "../../include/utils.h"

#include "tty_conf.h"

char* program_name = 0;

int main(int argc, char** argv)
{
    get_program_name(argv[0]);

    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw tty mode");

    // Very funny if that here is missing (bad tho :\)
    // *resets terminal to original state*
    if (atexit(set_tty_atexit)) 
        err_sys("atexit failed");
    
}

