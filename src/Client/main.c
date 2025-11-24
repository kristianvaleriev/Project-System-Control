#include "../../include/includes.h"
#include "../../include/utils.h"
#include "../../include/network.h"

#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>

#include "tty_conf.h"
#include "client_networking.h"

char* program_name = 0;

int     handle_connection_socket(char* server_addr, size_t addr_size);
void*   reading_server_function(void*);

int main(int argc, char** argv)
{
    set_program_name(argv[0]);
    info_msg("beginning multicast search");

    char server_addr[128];
    int multicast_status =  multicast_recv_def(server_addr, sizeof server_addr);
    if (multicast_status)
        err_sys("multicast receiver failed (err num: %d)", multicast_status);

    //print_ip_addr(&server_addr, "got server ip: %s", info_msg);
    info_msg("got server ip: %s", server_addr);
    int server_socket = handle_connection_socket(server_addr, sizeof server_addr);
    
    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw tty mode");

    // Very funny if that here is missing (bad tho :\)
    // *resets terminal to it's original state*
    if (atexit(set_tty_atexit)) 
        err_sys("atexit failed");

    pthread_t reading_thread;
    pthread_create(&reading_thread, NULL, reading_server_function,
                   (int[]) {server_socket, STDOUT_FILENO});

    while (1) 
    {
        char buf[128] = {0};
        ssize_t rc = read(STDIN_FILENO, buf, sizeof buf);
        if (rc < 0)
            err_info("read failed");

        if (send(server_socket, buf, rc, MSG_NOSIGNAL) < 0)
            break;
    }
    info_msg("exiting...\n\n");


    exit(0);
}

int handle_connection_socket(char* server_addr, size_t addr_size) 
{
    int server_socket = get_connected_socket(server_addr, addr_size);
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

    info_msg("connection successful\n\n");
    return server_socket;
}

void* reading_server_function(void* fds)
{
    int* server_socket = (int*) fds;
    int* write_fd      = (int*) (fds + sizeof(int));

    ssize_t size;
    char buf[4098];
    while (1)
    {
        if ((size = recv(*server_socket, buf, sizeof buf, 0)) < 0) {
            err_info("reading function's read fail");
            continue;;
        }
        else if (!size) {
            info_msg("Server connection's off. Exiting...\n\r");
            exit(0);
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
