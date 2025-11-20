#include "../../include/includes.h"
#include "../../include/utils.h"
#include "../../include/network.h"

#include <errno.h>
#include <sys/socket.h>

#include "tty_conf.h"

char* program_name = 0;

int main(int argc, char** argv)
{
    set_program_name(argv[0]);

    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw tty mode");

    // Very funny if that here is missing (bad tho :\)
    // *resets terminal to original state*
    if (atexit(set_tty_atexit)) 
        err_sys("atexit failed");
    
    struct addrinfo hint, *result, *temp_ai;
    memset(&hint, 0, sizeof hint);
    hint.ai_family   = AF_UNSPEC;  // Either IPv4 or v6, doesn't mather.
    hint.ai_socktype = SOCK_STREAM;// TCP please.
    hint.ai_flags    = AI_PASSIVE; // Give me an IP.

    if ((errno = getaddrinfo(NULL, KNOWN_PORT, &hint, &result))) 
        return -1;

    int socket_fd;
    for (temp_ai = result; temp_ai; temp_ai = temp_ai->ai_next)
    {
        if ((socket_fd = socket(temp_ai->ai_family, temp_ai->ai_socktype, temp_ai->ai_protocol)) > -1) 
        {
            if (connect(socket_fd, temp_ai->ai_addr, temp_ai->ai_addrlen))
                break;
        }
    }
    if (!temp_ai)
        err_sys("failed to connect");

    puts("connected!");
    sleep(5);
}

