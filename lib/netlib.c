#include "../include/network.h"

#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/uio.h>
#include <syslog.h>
#include <ctype.h>

void* convert_addr(struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET)
        return &(((struct sockaddr_in*) addr)->sin_addr);
    return &(((struct sockaddr_in6*) addr)->sin6_addr);
}

void print_ip_addr(void* sock_addr, char* msg, 
                   void (*print_fn)(const char* fmt, ...))
{
    struct sockaddr* addr = (struct sockaddr*) sock_addr;

    char buf[128] = {0};
    if (inet_ntop(addr->sa_family,
              convert_addr(addr),
              buf, sizeof buf))
        print_fn(msg, buf);
}
/*
 * The standard send() function can return even if only a part of the data 
 * was sent. In that case it's our responsibility to send the rest 
 * (because ofc it's ours, that is the Unix way ("This is the way.")).
*/
ssize_t sendall(int socket, void* buf, size_t bytes, int flags)
{
    ssize_t sent  = 0;
    size_t offset = 0;
    while ((sent = send(socket, buf + offset, bytes, flags) != (ssize_t) bytes))
    {
        if (sent < 0)
            break;
        offset += sent;
        bytes  -= offset;
    }

    return (offset) ? -1 : sent;
}

// MULTICAST FUNCTIONS AND MACROS
/*
* Here the client and the server change places. Firstly the server sends a msg to all listing 
* clients in a particular group of the local network which contains it's IP address and AF.
* The clients recvs it, search for that IP by DNS (getaddrinfo), and connects it.
*
* Server - send();
* Client - recv();
*
* This code was written by me so so long ago... (january 2025 :)) that I have forgotten most of it
* and I am too lazy and too out of time for rewrite or even looking at it so I hope most of it 
* is fine (although it looks terrible). So far it works, how? Idk, I forgot. Maybe one day I will
* upgrade it.
*/
#define MULTICAST_ADDR "239.255.255.250"
#define MULTICAST_PORT 16131

static int    multicast_sock = -1;
static void*  multicast_addr = 0;
static size_t multicast_len  = 0;


int multicast_init(char* addr, int domain, int port)
{
    
    if (!addr || !domain) {
        addr   = MULTICAST_ADDR;
        domain = AF_INET;
    }
    if (!port)
        port = MULTICAST_PORT;

    multicast_sock = socket(domain, SOCK_DGRAM, 0);
    if (multicast_sock < 0)
        return -1;

    if (domain == AF_INET) {
        multicast_len  = sizeof (struct sockaddr_in);

        struct sockaddr_in* in_buf = malloc(multicast_len);
        memset(in_buf, 0, multicast_len);
        in_buf->sin_family = AF_INET;
        in_buf->sin_port   = htons(port);

        if (inet_pton(domain, addr, &in_buf->sin_addr) != 1)
            return -1;
        //if (setsockopt(multicast_sock, IPPROTO_IP, IP_MULTICAST_IF, 
        //              &in_buf->sin_addr, sizeof (in_buf->sin_addr)) < 0)
        //    return -1;

        multicast_addr = in_buf;
    } 
    else {
        multicast_len  = sizeof (struct sockaddr_in6);

        struct sockaddr_in6* in6_buf = malloc(multicast_len);
        memset(in6_buf, 0, multicast_len);
        in6_buf->sin6_family = AF_INET6;
        in6_buf->sin6_port   = htons(port);

        if (inet_pton(domain, addr, &in6_buf->sin6_addr) != 1)
            return -1;

        multicast_addr = in6_buf;
    }
    //u_char loop = 0;
    //setsockopt(multicast_sock, IPPROTO_IP, IP_MULTICAST_LOOP,
    //           &loop, sizeof(loop));
    return 0;
}

void multicast_dealloc() {
    free(multicast_addr);
    close(multicast_sock);

    multicast_len = 0;
    multicast_addr = 0;
    multicast_sock = -1;
}

int multicast_send(void* msg, size_t msg_size)
{
    if (sendto(multicast_sock, msg, msg_size, 0, 
               (struct sockaddr*) multicast_addr, multicast_len) < 0) 
        return -1;

    return 0;
}

int multicast_recv(char* group, int domain, int port, 
                   void* buf, size_t buf_len)
{
    if (!domain || !group) {
        domain = AF_INET;
        group  = MULTICAST_ADDR;
    }
    if (!port)
        port = MULTICAST_PORT;

    int recv_socket = socket(domain, SOCK_DGRAM, 0);
    if (recv_socket < 0)
        return -1;

    int yes = 1;
    if (setsockopt(recv_socket, SOL_SOCKET, SO_REUSEADDR, 
                   &yes, sizeof yes) < 0)
        return -1;

    void* addr;
    socklen_t addr_len;

    if (domain == AF_INET) {
        volatile struct sockaddr_in in_buf;
        memset(&in_buf, 0, sizeof in_buf);
        in_buf.sin_family = AF_INET;
        in_buf.sin_port   = htons(port);
        in_buf.sin_addr.s_addr = htonl(INADDR_ANY);

        addr = &in_buf;
        addr_len = sizeof in_buf;
    } 
    else {
        volatile struct sockaddr_in6 in6_buf;
        memset(&in6_buf, 0, sizeof in6_buf);
        in6_buf.sin6_family = AF_INET6;
        in6_buf.sin6_port   = htons(port);
        in6_buf.sin6_addr   = in6addr_any;

        addr = &in6_buf;
        addr_len = sizeof in6_buf;
    }

    if (bind(recv_socket, (struct sockaddr*) addr, addr_len) < 0)
        return -2;

    if (domain == AF_INET) {
        struct ip_mreq mreq;
        if (inet_pton(AF_INET, group, &mreq.imr_multiaddr) != 1) 
            return -3;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (setsockopt(recv_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                       &mreq, sizeof mreq) < 0)
            return -3;

        //mreq_ptr  = &mreq;
        //mreq_size = sizeof mreq; 
    }
    else if (domain == AF_INET6){
        struct ipv6_mreq mreq6;
        if (inet_pton(AF_INET6, group, &mreq6.ipv6mr_multiaddr) != 1)
            return -3;

        if (setsockopt(recv_socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, 
                  (char*) &mreq6, sizeof mreq6) < 0)
            return -3;

        //mreq_ptr  = &mreq6;
        //mreq_size = sizeof mreq6;
    }
    else return -3;
    
    ssize_t recv_bytes = recvfrom(recv_socket, buf, buf_len, 0, 
                                  (struct sockaddr*) addr, &addr_len);
    if (recv_bytes < 0) 
        return -4;

    if (recv_bytes < buf_len)
        ((char*) buf)[recv_bytes] = '\0'; //pain...necessary

    close(recv_socket);
    return 0;
}


// UTILS

