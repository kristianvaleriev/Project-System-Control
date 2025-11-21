#ifndef _CLIENT_NET_H_
#define _CLIENT_NET_H_

#include <sys/types.h>

#define ERR_GETADDRINFO -1
#define ERR_SOCKET      -2
#define ERR_CONNECT     -3

int   get_connected_socket(void* addr, size_t);

#endif
