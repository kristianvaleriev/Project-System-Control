/*
* Here reside common network functions for both the server and the client
*
*/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <sys/types.h> // My blasted LSP requires it, else there are lots of errors. Remove at the end.
#include <netdb.h>

#define KNOWN_PORT "15131"

char*  get_host(int socktype, int addr_family, char* buf, size_t buf_len);

int    multicast_init(char* addr, int domain, int port);
void   multicast_dealloc(void);
int    multicast_send(char* msg);
int    multicast_recv(char* group, int domain, int port, 
                      char* buf, size_t buf_len);

#define multi_def_recv(buf, len) multicast_recv(0, 0, 0, buf, len)

void*   convert_addr(struct sockaddr* addr);

ssize_t sendall(int socket, void* buf, size_t bytes, int flags);
int     send_errmsg(int fd, char* msg);
int     send_errno(int fd);

#endif
