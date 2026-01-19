#include "../../include/includes.h"
#include "../../include/utils.h"
#include "../../include/network.h"

#include <asm-generic/errno.h>
#include <bits/getopt_core.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <termios.h>
#include <ncurses.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "tty_conf.h"
#include "dynamic_files.h"
#include "client_networking.h"

#define DEF_PROG "dmesg --follow -H"

#define SHORT_ARGS "-a:f:d:np::"
static const struct option long_options[] = {
    { "address",  required_argument, NULL, 'a'},
    { "drivers",  required_argument, NULL, 'd'},
    { "files",    required_argument, NULL, 'f'},
    { "program",  optional_argument, NULL, 'p'},
    { "no-ncurses", no_argument, NULL, 'n'},
    {},
};

#ifndef WITHOUT_NCURSES
    #include "client_ncurses.h"
    static int using_ncurses = 1;
    
/*
    static pthread_t ncurses_thread;

    extern pthread_mutex_t setup_lock;
    extern pthread_cond_t setup_cond;

    extern int is_setup_done;
*/

#else 
    static int using_ncurses = 0;

#endif

static pthread_t reading_thread;
static pthread_mutex_t reading_thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned short  thread_count;

volatile sig_atomic_t tty_resized = 1;

char* program_name = 0;
char* program_storage = (char*) -1;

struct read_thd_args {
    int socket;
    int write_fd;
    int exit_flag;
};

void    save_to_file(char*, void*, size_t);
int     handle_connection_socket(char* server_addr, size_t addr_size);
void*   reading_server_function(void*);
void    setup_dedicated_program(char* name);
void    send_winsize_info(int);
void    set_signals(void);
void    main_cmd_loop(int);

int main(int argc, char** argv)
{
    char* server_addr = NULL;
    char* dedicated_program = strdup(DEF_PROG);

    set_program_name(argv[0]);

    struct filename_array* drivers = init_filename_array();
    struct filename_array* files   = init_filename_array();

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

        case 'p': {
            free(dedicated_program);
            dedicated_program = NULL;
        }
        break;

        case 'n': using_ncurses = 0;
        break;

        case 1:
            switch (temp) {
            case 'f': insert_in_array(files, optarg, strlen(optarg));
            break;

            case 'd': insert_in_array(drivers, optarg, strlen(optarg));
            break;

            case 'p': 
                if (dedicated_program) {
                    dedicated_program = realloc(dedicated_program, strlen(optarg) + 
                                                strlen(dedicated_program) + 2);
                    strcat(dedicated_program, " ");
                } 
                else 
                    dedicated_program = calloc(strlen(optarg) + 1, 1);

                strcat(dedicated_program, optarg);
            break;

            default: info_msg("unrecognized option's argument");
            break;
            }
            
        continue;
        }

        temp = ch_arg;
    }
}

    if (set_tty_raw(STDIN_FILENO) < 0) 
        err_sys("could not set raw tty mode");
    // Very funny if that here is missing (bad tho :\)
    // *resets terminal to it's original state*
    if (atexit(set_tty_atexit))
        err_sys("atexit failed");

    set_signals();

#ifndef WITHOUT_NCURSES
    if (using_ncurses) {
        /*
        int status = pthread_create(&ncurses_thread, NULL, setup_client_ncurses, NULL);
        if (status)
            err_cont(status, "Client setup of ncurses library failed! Continuing...");
    
        pthread_mutex_lock(&setup_lock);
        while (!is_setup_done)
            pthread_cond_wait(&setup_cond, &setup_lock);
        pthread_mutex_unlock(&setup_lock);
        */

        int err_fd = open(LOG_NAME, O_RDWR | O_CREAT | O_TRUNC, 0755);
        if (err_fd >= 0) {
            dup2(err_fd, STDERR_FILENO);
            close(err_fd);
        }
        else err_info("could not create err.log! Sorry...");

        struct winsize win; 
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win) < 0) 
            err_sys("error on retriving window size");

        struct window_placement interface[] = {
            {
                .fd = STDERR_FILENO,
                .type = PANE,
                .coords = {
                    .rows = win.ws_row * 0.25,
                    .cols = win.ws_col,
                    .starty = win.ws_row - win.ws_row * 0.25,
                    .startx = 0,
                },
                .border = {' ', ' ', 0, ' ', ' ', ' ', ' ', ' '},
            },
            {
                .fd = STDOUT_FILENO, 
                .type = MAIN,
                .border = {' '},
            },
        };

        size_t count = sizeof interface / sizeof *interface;
        if (!handle_ncurses_and_fork(interface, count))
            setup_client_ncurses(interface, count);
    } 
#endif

    if (!server_addr) 
    {
        server_addr = calloc(INET6_ADDRSTRLEN, 1);
        if (!server_addr)
            err_sys("malloc failed");

        info_msg("beginning multicast search...");

        int multicast_status =  multicast_recv_def(server_addr, INET6_ADDRSTRLEN);
        if (multicast_status)
            err_sys("multicast receiver failed (err num: %d)", multicast_status);

        info_msg("got server ip: %s", server_addr);
        save_to_file("received_server_ip", server_addr, strlen(server_addr));
    }

    int server_socket = handle_connection_socket(server_addr, sizeof server_addr);
    
    fork_handle_file_send(TYPE_FILES, files);
    dealloc_filename_array(files);

    fork_handle_file_send(TYPE_DRIVERS, drivers);
    dealloc_filename_array(drivers);

    if (using_ncurses && dedicated_program) 
        setup_dedicated_program(dedicated_program);

    free(server_addr);


    set_nonblocking(STDIN_FILENO);
    set_nonblocking(server_socket);

    struct read_thd_args* args = malloc(sizeof *args);
    args->socket    = server_socket;
    args->write_fd  = STDOUT_FILENO;
    args->exit_flag = 1;

    pthread_create(&reading_thread, NULL, reading_server_function, (void*) args);
    main_cmd_loop(server_socket);


    info_msg("exiting...\n\n");
    exit(0);
}

void main_cmd_loop(int server_socket)
{
    char buf[128] = {0};

    size_t offset = sizeof(struct client_request);
    size_t sizeof_buf = (sizeof buf) - offset;

    char*  write_buf = buf + offset;

    ssize_t rc;
    while (1) 
    {
        if (tty_resized) {
            tty_resized = 0;
            send_winsize_info(server_socket);
        }

    #ifdef ACCUMULATIVE_READ
        rc = accumulative_read(STDIN_FILENO, write_buf, sizeof_buf, 30, 1);
    #else
        rc = read(STDIN_FILENO, write_buf, sizeof_buf);
    #endif

        if (rc <= 0) {
            if (rc && errno != EWOULDBLOCK) 
                err_info("read failed");
            
            continue;
        }
        
        if (send(server_socket, buf, rc + offset, MSG_NOSIGNAL) <= 0)
            if (errno != EWOULDBLOCK)
                break;
    }
}

// Tried to use the util threaded function, but it is too
// different and it would be a mess.
void* reading_server_function(void* arg)
{
    struct read_thd_args* args = arg;
    int server_socket = args->socket;
    int write_fd      = args->write_fd;
    int flag          = args->exit_flag;

    free(args);

    pthread_mutex_lock(&reading_thread_mutex);
    thread_count++;
    pthread_mutex_unlock(&reading_thread_mutex);

    sleep(1);

    ssize_t rc;
    char buf[1024] = {0};
    while (1)
    {
        if ((rc = recv(server_socket, buf, sizeof buf, 0)) <= 0) {
            if (!rc) {
                if (flag) {
                    info_msg("server connection's off. Exiting...");
                    exit(0);
                }
                else break;
            }

            if (errno != EWOULDBLOCK)
                err_info("reading function's recv fail");
            continue;
        }
        //buf[rc] = '\0';
        //info_msg("buf: %s", buf);

        if ((rc = writeall(write_fd, buf, rc)) < 0)  {
            if (errno != EWOULDBLOCK)
                err_sys("reading function's write fail");
        }
    }

    pthread_mutex_lock(&reading_thread_mutex);
    thread_count--;
    pthread_mutex_unlock(&reading_thread_mutex);

    return 0;
}


void send_winsize_info(int server_socket)
{
    if (!isatty(STDIN_FILENO))
        return;

    struct winsize win; 
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) < 0) {
        err_info("error on retriving window size");
        return;
    }

    // Hack...
    if (!win.ws_row) win.ws_row = 250;
    if (!win.ws_col) win.ws_col = 500;

//    info_msg("sending terminal size of %d rows and %d cols", 
//             win.ws_row, win.ws_col);

    struct client_request req = {
        .data_size = htonl(sizeof win),
        .type      = htonl(TYPE_WINSIZE),
    };

    if (sendall(server_socket, &req, sizeof req, 0) < 0 ||       
        sendall(server_socket, &win, sizeof win, 0) < 0)
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

void setup_dedicated_program(char* prog_name)
{
    int dedicated_socket = new_connected_server_socket();
    size_t prog_len = strlen(prog_name) + 1;
    pthread_t rthread;


    struct client_request req = {
        .data_size = htonl(prog_len),
        .type = htonl(TYPE_PROGRAM),
    };

    if (sendall(dedicated_socket, &req, sizeof req, 0) < 0 ||
        sendall(dedicated_socket, prog_name, prog_len, 0) < 0) {
        err_info("sendall failed in data send of a dedicated program");

        return;
    }

    send_winsize_info(dedicated_socket);

    struct read_thd_args* args = malloc(sizeof *args);
    args->socket    = dedicated_socket;
    args->write_fd  = STDERR_FILENO;
    args->exit_flag = 0;

    pthread_create(&rthread, NULL, reading_server_function, (void*) args);
}

static void sig_winch_handle(int _) { tty_resized = 1; }

void set_signals(void)
{
    struct sigaction sa;
    sa.sa_handler = sig_winch_handle;
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
