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

    struct sockaddr server_addr;
    int multicast_status=  multicast_recv_def(&server_addr, sizeof server_addr);
    if (multicast_status)
        err_sys("multicast recv failed (err num: %d)", multicast_status);

    print_ip_addr(&server_addr, "got server ip: %s", info_msg);

    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw tty mode");

    // Very funny if that here is missing (bad tho :\)
    // *resets terminal to original state*
    if (atexit(set_tty_atexit)) 
        err_sys("atexit failed");


}

