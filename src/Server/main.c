#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <asm-generic/errno-base.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include <sys/wait.h>
#include <sys/poll.h>
#include <syslog.h>

#include "networking.h"
#include "client_handling.h"

void fork_for_client(int,int);
void sigchld_handler(int);
void set_signals(void);

char* program_name = 0;

int main(int argc, char** argv)
{
    struct addrinfo server_ip;

    set_program_name(argv[0]);
    init_syslog(program_name, LOG_CONS, LOG_DAEMON);
        
    int listening_socket = get_listening_socket(&server_ip);
    if (listening_socket < 0) {
        if (listening_socket == -1) 
            err_quit_msg("getaddrinfo error: %s", gai_strerror(errno));
        else 
            err_sys("could not get a listening socket (err num=%d)", listening_socket);
    }
            
    print_ip_addr(server_ip.ai_addr, "server's ip address: %s", info_msg);
    set_signals();

    struct sockaddr_storage client_addr = {0};
    socklen_t sockaddr_size;
    /*
     * Server's main loop accepts new connections from users wanting to use 
     * the host system and makes a child process that will handle the rest of the
     * user's needs and communication.
     */
    for (;/* main loop */;)
    {
        int client_socket = accept(listening_socket, 
                                   (struct sockaddr*) &client_addr,
                                   &sockaddr_size);
        if (client_socket < 0)
            err_sys("accept failed");

        print_ip_addr((struct sockaddr*) &client_addr, 
                      "got connection from %s", info_msg);  

        fork_for_client(listening_socket, client_socket);
        close(client_socket);
    }
}

void fork_for_client(int listening_socket, int client_socket)
{
    pid_t pid = fork();
    if (pid) {
        if (pid < 0)
            err_info("fork");

        // parent returns as if nothing has happened
        return; 
    }
    // Now the child is in da house.

    /*
     * Not used by the child process, but it takes resources
     * so we give it to the function to make posible freeing it.
     */
    close(listening_socket); 

    handle_client(client_socket);

    // Standard exit of child.
    exit(0);
}

/*
* In Unix like OSes if child processes terminate before the parent does,
* they become (literaly that's how they are called) *Zombies*, which like
* the real ones, occupy resources that can be better spend by the system.
* 
* That is because the parent would maybe want to know what happened to its
* children, so the kernel keeps that data until retrived. As any good parent
* this server doesnt care what happened to them, but it's responsible enough 
* to free the resources so the admin doesnt literaly kill it.
*/ 
void sigchld_handler(int _)
{
    int errnum = errno; // might be overwritten by waitpid
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = errnum;
}

// Handle signals of interest
void set_signals(void)
{
    struct sigaction sig_chld;
    sig_chld.sa_handler = sigchld_handler;
    sig_chld.sa_flags   = SA_RESTART; // restart interupted syscalls
    sigemptyset(&sig_chld.sa_mask);   // no need to turn off signals

    if (sigaction(SIGCHLD, &sig_chld, NULL) < 0)
        err_sys("sigaction failed");
}
