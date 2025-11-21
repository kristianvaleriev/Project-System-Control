#include "../../include/includes.h"
#include "../../include/utils.h"
#include "../../include/network.h"

#include <errno.h>
#include <sys/socket.h>

#include "tty_conf.h"
#include "client_networking.h"

char* program_name = 0;

int main(int argc, char** argv)
{
    set_program_name(argv[0]);
    info_msg("beginning multicast search");

    char server_addr[128];
    int multicast_status=  multicast_recv_def(server_addr, sizeof server_addr);
    if (multicast_status)
        err_sys("multicast recv failed (err num: %d)", multicast_status);

    //print_ip_addr(&server_addr, "got server ip: %s", info_msg);
    info_msg("got server ip: %s", server_addr);

    int server_socket = get_connected_socket(server_addr, sizeof server_addr);
    if (server_socket < 0) {
        if (server_socket == -1)
            err_quit_msg("getaddrinfo error: %s", gai_strerror(server_socket));
        else 
            err_sys("could not connect to server (err num: %d)", server_socket);
    }
    info_msg("connection successful!");

    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw tty mode");

    // Very funny if that here is missing (bad tho :\)
    // *resets terminal to original state*
    if (atexit(set_tty_atexit)) 
        err_sys("atexit failed");

}

