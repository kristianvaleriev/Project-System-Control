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
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>

int get_listening_socket(struct addrinfo* binded_ai)
{
    struct addrinfo hint, *result, *temp_ai;
    memset(&hint, 0, sizeof hint);
    hint.ai_socktype = SOCK_STREAM;// TCP please.
    hint.ai_family   = AF_UNSPEC;  // Either IPv4 or v6, doesn't mather.
    hint.ai_flags    = AI_PASSIVE; // Give me an IP.

    if ((errno = getaddrinfo(NULL, KNOWN_PORT, &hint, &result))) 
        return -1;

    int socket_fd;
    for (temp_ai = result; temp_ai; temp_ai = temp_ai->ai_next)
    {
        // Maybe it would look better if I had used continue statements
        // but that's like 2-3 more lines of code so nah.
        if ((socket_fd = socket(temp_ai->ai_family, temp_ai->ai_socktype, temp_ai->ai_protocol)) > -1) 
        {
            if ( ! setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, 
                   /*funny-*/ &(int){1}, temp_ai->ai_protocol)) 
            {
                if ( ! bind(socket_fd, temp_ai->ai_addr, temp_ai->ai_addrlen)) 
                {
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

/*
* Return the IP address of one working, not loopback
* network interface. The server listens on all net 
* interfaces so it doesnt matter which one (I guess
* ofc, I am not an expert).
*/
char* get_host_addr(struct sockaddr* addr)
{
    char* ret_addr = 0;
    struct ifaddrs* result, *ptr;
    getifaddrs(&result);

    for (ptr = result; ptr; ptr=ptr->ifa_next)
    {
        if (!(ptr->ifa_flags & IFF_UP) ||
              ptr->ifa_flags & IFF_LOOPBACK ||
            !(strcmp(ptr->ifa_name, "lo")))
            continue;

        struct sockaddr* saddr = ptr->ifa_addr;

        if (saddr->sa_family == AF_INET)
            ret_addr = malloc(INET_ADDRSTRLEN);
        else if (saddr->sa_family == AF_INET6)
            ret_addr = malloc(INET6_ADDRSTRLEN);
        else 
            continue;

        if (inet_ntop(saddr->sa_family, 
                      convert_addr(saddr),
                      ret_addr, INET6_ADDRSTRLEN))
            break;
        
        free(ret_addr);
        ret_addr = 0;
    }

    if (ret_addr && addr) 
        *addr = *ptr->ifa_addr;

    freeifaddrs(result);
    return ret_addr;
}

void* multicast_beacon(void* _)
{
    struct sockaddr server_addr;
    char* p_addr = get_host_addr(&server_addr);

    // Here is hoping nobody notices this race cond.
    // (probably fine if using syslog)
    info_msg("server's ip address: %s\n", p_addr);

    multicast_init_def();
    while (1) {
      #ifdef SEND_VIA_SOCKADDR // experiments
        multicast_send(&server_addr, sizeof server_addr);
      #else
        multicast_send(p_addr, strlen(p_addr));
      #endif
        sleep(2);
    }

    // Probably wont happened but still...
    multicast_dealloc(); 
    return 0;
}
