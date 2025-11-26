/*
* Here reside common network functions for both the server and the client
*
*/

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <sys/types.h> // My blasted LSP requires it, else there are lots of errors. Remove at the end.
#include <netdb.h>

#define KNOWN_PORT "7932"

int    multicast_init(char* addr, int domain, int port);
void   multicast_dealloc(void);
int    multicast_send(void* msg, size_t);
int    multicast_recv(char* group, int domain, int port, 
                      void* buf, size_t buf_len);

#define multicast_init_def() multicast_init(0,0,0)
#define multicast_recv_def(buf, len) multicast_recv(0, 0, 0, buf, len)

// Utils
void*   convert_addr(struct sockaddr* addr);
void    print_ip_addr(void* sock_addr, char* msg, 
                      void (*print_fn)(const char* fmt, ...));
ssize_t sendall(int socket, void* buf, size_t bytes, int flags);


// Request protocol

typedef enum {
    TYPE_COMMAND = 0,
    TYPE_FILES,
    TYPE_DRIVERS,
    TYPE_WINSIZE,
} reqtype_e; 

struct client_request {
    uint32_t data_size; // 0 if shell cmd
    reqtype_e type; 
};

#endif
