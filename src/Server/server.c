#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "networking.h"

char* program_name = 0;

int main(int argc, char** argv)
{
    struct addrinfo server_ip;

    get_program_name(argv[0]);
        
    int listening_socket = get_listening_socket(&server_ip);
    if (listening_socket < 0) {
        if (listening_socket == -1) 
            err_quit_msg("getaddrinfo error: %s", gai_strerror(errno));
        else 
            err_sys("Could not get a listening socket (err_n=%d)", listening_socket);
    }
            
    char buf[128] = {0};
    inet_ntop(server_ip.ai_family,
              convert_addr(server_ip.ai_addr),
              buf, sizeof buf);
<<<<<<< HEAD
        
=======

    info_msg("Server's ip is: %s", buf);
>>>>>>> 0cd131b46e505dee919cc02374a35b20febcc209
}
