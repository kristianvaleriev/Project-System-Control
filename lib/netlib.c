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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

char* get_host(int socktype, int addr_family, char* buf, size_t buf_len)
{
    char* addr_ret = 0;

    long n = sysconf(_SC_HOST_NAME_MAX);
    if (n < 0) 
        n = HOST_NAME_MAX;

    char* str = malloc(n);
    if (!str) 
        return 0;
    if (gethostname(str, n) < 2) {
        goto RETURN;
    }

    struct addrinfo hint;
    memset(&hint, 0, sizeof hint);
    hint.ai_flags    = AI_CANONNAME;
    hint.ai_family   = addr_family ? addr_family : AF_UNSPEC;  
    if (socktype)
        hint.ai_socktype = socktype;

    struct addrinfo* r_addr = 0;
    int rc = getaddrinfo(str, 0, &hint, &r_addr);
    if (rc || !r_addr) 
        goto RETURN;

    if (buf && buf_len) {
        size_t str_len = strlen(str)+1;
        if (buf_len >= str_len)
            memcpy(buf, str, str_len);
        else 
            *buf = 0;
    }

    addr_ret = malloc(INET6_ADDRSTRLEN);
    if (!addr_ret) {
        freeaddrinfo(r_addr);
        goto RETURN;
    }

    //return (char*) {"a"};
    if (inet_ntop(r_addr->ai_family,  
                  convert_addr(r_addr->ai_addr),
                  addr_ret, INET6_ADDRSTRLEN) == 0) {
        free(addr_ret);
        addr_ret = 0;
    }
    freeaddrinfo(r_addr);

  RETURN:
    free(str);
    return addr_ret;
}

// MULTICAST FUNCTIONS AND MACROS
/*
* Here the client and the server change places. Firstly the server sends a msg to all listing 
* clients in a particular group of the local network which contains it's IP address and AF.
* The clients recvs it, search for that IP by DNS (getaddrinfo), and connects it.
*
* Server - send();
* Client - recv();
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

int multicast_send(char* msg)
{
    if (multicast_sock < 0 || !multicast_addr || !multicast_len) 
        if (multicast_init(0, 0, 0) < 0)
            return -1;

    if (sendto(multicast_sock, msg, strlen(msg), 0, 
               (struct sockaddr*) multicast_addr, multicast_len) < 0) 
        return -1;

    return 0;
}

int multicast_recv(char* group, int domain, int port, 
                   char* buf, size_t buf_len)
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
        struct sockaddr_in in_buf;
        memset(&in_buf, 0, sizeof in_buf);
        in_buf.sin_family = AF_INET;
        in_buf.sin_port   = htons(port);
        in_buf.sin_addr.s_addr = htonl(INADDR_ANY);

        addr = &in_buf;
        addr_len = sizeof in_buf;
    } 
    else {
        struct sockaddr_in6 in6_buf;
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
    buf[recv_bytes] = '\0';

    close(recv_socket);
    return 0;
}


// UTILS

void* convert_addr(struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET)
        return &(((struct sockaddr_in*) addr)->sin_addr);
    return &(((struct sockaddr_in6*) addr)->sin6_addr);
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
    while ((sent = send(socket, buf + offset, bytes, flags) != bytes))
    {
        if (sent < 0)
            break;
        offset += sent;
        bytes  -= offset;
    }

    return (offset) ? sent : -1;
}
