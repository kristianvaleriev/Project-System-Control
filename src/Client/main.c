#include "../../include/includes.h"
#include "../../include/utils.h"
#include "../../include/network.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>

#include "tty_conf.h"
#include "client_networking.h"

char* program_name = 0;

void    main_cmd_loop(int server_socket);
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

    struct winsize wins; 
    ioctl(STDIN_FILENO, TIOCGWINSZ, &wins);
    struct client_request req = {
        .data_size = htonl(sizeof wins),
        .type      = htonl(TYPE_WINSIZE),
    };

    info_msg("col & rows: %d %d\n\r", wins.ws_col, wins.ws_row);
    sendall(server_socket, &req,  sizeof req, 0);
    sendall(server_socket, &wins, sizeof wins, 0);
    
    pthread_t reading_thread;
    pthread_create(&reading_thread, NULL, reading_server_function,
                   (int[]) {server_socket, STDOUT_FILENO});

    main_cmd_loop(server_socket);


    info_msg("exiting...\n\n");
    exit(0);
}

void main_cmd_loop(int server_socket)
{
    char buf[128] = {0};
    size_t offset = sizeof(struct client_request);
    char*  write_buf = buf + offset;
    size_t sizeof_buf = (sizeof buf) - offset;

    ssize_t rc;
    while (1) 
    {
        rc = read(STDIN_FILENO, write_buf, sizeof_buf);
        if (rc < 0)
            err_info("read failed");

        if (send(server_socket, buf, rc + offset, MSG_NOSIGNAL) < 0)
            break;
    }
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
