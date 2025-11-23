#include "../../include/includes.h"
#include "../../include/utils.h"
#include "../../include/network.h"

#include <sys/socket.h>
#include <unistd.h>

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
        err_sys("multicast receiver failed (err num: %d)", multicast_status);

    //print_ip_addr(&server_addr, "got server ip: %s", info_msg);
    info_msg("got server ip: %s", server_addr);

    int server_socket = get_connected_socket(server_addr, sizeof server_addr);
    if (server_socket < 0) 
    {
        if (server_socket == ERR_GETADDRINFO) {
            err_quit_msg("getaddrinfo error: %s", gai_strerror(server_socket));
        }
        else if (server_socket == ERR_CONNECT){
            err_sys("connection call failed");
        }
        else 
            err_sys("error on connection try (err num: %d)", server_socket);
    }

    info_msg("connection successful");

    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw tty mode");

    // Very funny if that here is missing (bad tho :\)
    // *resets terminal to it's original state*
    if (atexit(set_tty_atexit)) 
        err_sys("atexit failed");

    while (1) 
    {
        char buf[128] = {0};
        ssize_t rc = read(STDIN_FILENO, buf, sizeof buf);
        if (rc < 0)
            err_info("read failed");
        if (sendall(server_socket, buf, rc, 0) < 0)
            err_info("sendall failed");

        if (buf[0] == '\r') {
            sleep(3);
            break;
        }
    }

    set_tty_user(STDIN_FILENO);
    while (1)
    {
        char buf[128] = {0};
        recv(server_socket, buf, sizeof buf, 0);
        info_msg("recv buf: %.1000s", buf);
    }
}

