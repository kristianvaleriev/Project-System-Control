#include "../../include/includes.h"
#include "../../include/utils.h"
#include "../../include/network.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <getopt.h>

#include "tty_conf.h"
#include "client_networking.h"
#include "dynamic_files.h"

#define SHORT_ARGS "-a:f:d:"
static struct option long_options[] = {
    { "address",  required_argument, NULL, 'a'},
    { "drivers",  required_argument, NULL, 'd'},
    { "files",    required_argument, NULL, 'f'},
    {},
};

static int __server_socket = -1;
static pthread_t reading_thread;

char* program_name = 0;
char* program_storage = (char*) -1;


void    save_to_file(char*, void*, size_t);
int     handle_connection_socket(char* server_addr, size_t addr_size);
void*   reading_server_function(void*);
void    send_winsize_info(int);
void    set_signals(void);
void    main_cmd_loop(int);

int main(int argc, char** argv)
{
    char* server_addr = NULL;
    struct filename_array* drivers = init_filename_array();
    struct filename_array* files   = init_filename_array();

    set_program_name(argv[0]);

{
    char ch_arg, temp = 0; 
    while ((ch_arg = getopt_long(argc, argv, SHORT_ARGS, long_options, NULL)) != -1)
    {
        switch (ch_arg) {
        case 'a': 
            if (validate_ip_address(optarg))
                server_addr = strdup(optarg);
            else 
                err_quit_msg("given an invalid ip address");
        break;

        case 'f': insert_in_array(files, optarg, strlen(optarg));
        break;

        case 'd': insert_in_array(drivers, optarg, strlen(optarg));
        break;

        case 1:
            if (temp == 'f')
                insert_in_array(files, optarg, strlen(optarg));
            else if (temp == 'd')
                insert_in_array(drivers, optarg, strlen(optarg));
            else 
                info_msg("unrecognized option's argument");
        continue;
        }

        temp = ch_arg;
    }
}
    if (!server_addr) 
    {
        server_addr = calloc(INET6_ADDRSTRLEN, 1);
        if (!server_addr)
            err_sys("malloc failed");

        info_msg("beginning multicast search");
        int multicast_status =  multicast_recv_def(server_addr, INET6_ADDRSTRLEN);
        if (multicast_status)
            err_sys("multicast receiver failed (err num: %d)", multicast_status);

        info_msg("got server ip: %s", server_addr);
        save_to_file("received_server_ip", server_addr, strlen(server_addr));
    }

    int server_socket = handle_connection_socket(server_addr, sizeof server_addr);
    __server_socket = server_socket; // no choice :(( 
    
    fork_handle_file_send(TYPE_FILES, files);
    dealloc_filename_array(files);

    fork_handle_file_send(TYPE_DRIVERS, drivers);
    dealloc_filename_array(drivers);
    
    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw tty mode");
    // Very funny if that here is missing (bad tho :\)
    // *resets terminal to it's original state*
    if (atexit(set_tty_atexit)) 
        err_sys("atexit failed");

    send_winsize_info(0);
    set_signals();
    
    free(server_addr);
    main_cmd_loop(server_socket);


    info_msg("exiting...\n\n");
    exit(0);
}

void main_cmd_loop(int server_socket)
{
    pthread_create(&reading_thread, NULL, reading_server_function,
                   (int[]) {server_socket, STDOUT_FILENO, 1});

    char buf[128] = {0};
    size_t offset = sizeof(struct client_request);
    size_t sizeof_buf = (sizeof buf) - offset;

    char*  write_buf = buf + offset;

    ssize_t rc;
    while (1) 
    {
    #ifdef ACCUMULATIVE_READ
            rc = accumulative_read(STDIN_FILENO, write_buf, sizeof_buf, 30, 1);
    #else
        if ((rc = read(STDIN_FILENO, write_buf, sizeof_buf)) <= 0) {
            if (rc)
                err_info("read failed");
            continue;
        }
    #endif

        if (send(server_socket, buf, rc + offset, MSG_NOSIGNAL) < 0)
            break;
    }
}

void send_winsize_info(int _)
{
    if (!isatty(STDIN_FILENO))
        return;

    struct winsize win; 
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win) < 0) {
        err_info("error on retriving window size");
        return;
    }
    struct client_request req = {
        .data_size = htonl(sizeof win),
        .type      = htonl(TYPE_WINSIZE),
    };

    //info_msg("col & rows: %d %d\n\r", wins.ws_col, wins.ws_row);
    if (sendall(__server_socket, &req, sizeof req, 0) < 0 ||       
        sendall(__server_socket, &win, sizeof win, 0) < 0)
        err_info("error on sending request for changing window size");
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
            err_sys("connection call failed (errno: %d)", errno);
        }
        else 
            err_sys("error on connection try (err num: %d)", server_socket);
    }

    info_msg("connection successful\n\n");
    return server_socket;
}

// Tried to use the util threaded function, but it is too
// different and it would be a mess.
void* reading_server_function(void* fds)
{
    int server_socket = *((int*) fds);
    int write_fd      = *((int*) (fds + sizeof(int)));

    ssize_t rc;
    char buf[2048] = {0};
    while (1)
    {
        if ((rc = recv(server_socket, buf, sizeof buf, 0)) <= 0) {
            if (!rc) {
                info_msg("Server connection's off. Exiting...\n\r");
                exit(0);
            }

            err_info("reading function's recv fail");
            continue;
        }
        //buf[rc] = '\0';
        //info_msg("sending buf: %s", buf);

        if (write(write_fd, buf, rc) < 0) {
            err_info("reading function's write fail");
            break;
        }
    }
    return 0;
}

void set_signals(void)
{
    struct sigaction sa;
    sa.sa_handler = send_winsize_info;
    sa.sa_flags   = SA_RESTART; // restart interupted syscalls
    sigemptyset(&sa.sa_mask);   // no need to turn off signals

    if (sigaction(SIGWINCH, &sa, NULL) < 0)
        err_sys("sigaction failed");
}

void save_to_file(char* filename, void* content, size_t size)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0744);
    if (fd < 0) {
        err_info("open() failed when saving to file");
        return;
    }

    if (write(fd, content, size) < 0) {
        err_sys("write() failed when saving to file");
        return;
    }

    close(fd);
}
