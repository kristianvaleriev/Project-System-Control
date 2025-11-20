#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

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
                if ( ! bind(socket_fd, temp_ai->ai_addr, temp_ai->ai_addrlen)) {
                    if (binded_ai) {
                        memcpy(binded_ai, temp_ai, sizeof *temp_ai);

                        if (temp_ai->ai_canonname) // stupid strdup doesnt check for NULL ptrs...
                            binded_ai->ai_canonname = strdup(temp_ai->ai_canonname);

                        struct sockaddr* var = malloc(sizeof *var);
                        *var = *temp_ai->ai_addr; // same as memcpy()
                        binded_ai->ai_addr = var;
                    }
                    break;
                }
            } 
            else socket_fd = -2;
        }
    }
    freeaddrinfo(result);

    if (socket_fd < 0)
        return socket_fd;
    if (!temp_ai) 
        return -3;

    if (listen(socket_fd, 10) < 0)
        return -4;
    
    return socket_fd;
}

void print_ip_addr(void* sock_addr, char* msg, void (*print_fn)(char* fmt, ...))
{
    struct sockaddr* addr = (struct sockaddr*) sock_addr;

    char buf[128] = {0};
    if (inet_ntop(addr->sa_family,
              convert_addr(addr),
              buf, sizeof buf))
        print_fn(msg, buf);
    else 
        err_info("inet_ntop error");
}
