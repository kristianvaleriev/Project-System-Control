#include "../../include/includes.h"
#include "../../include/network.h"

#include <asm-generic/socket.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>

int get_listening_socket(struct addrinfo* binded_ai)
{
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
            if ( ! setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, 
                   /*funny-*/ &(int){1}, temp_ai->ai_protocol)) 
            {
                if ( ! bind(socket_fd, temp_ai->ai_addr, temp_ai->ai_addrlen))
                    break;
            } 
            else socket_fd = -2;
        }
    }
    freeaddrinfo(result);

    if (socket_fd < 0)
        return socket_fd;
    if (!temp_ai) 
        return -3;

    if (binded_ai)
        memcpy(binded_ai, temp_ai, sizeof *temp_ai);

    if (listen(socket_fd, 10) < 0)
        return -4;
    
    return socket_fd;
}
