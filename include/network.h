#ifndef __NETWORK_H__
#define __NETWORK_H__

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

#define PORT "15131" // Well kwown port
#define MSG_OUT "END131"

// Multicast functions
char* get_host(int socktype, int addr_family, char* buf, size_t buf_len);

int  multicast_init(char* addr, int domain, int port);
void multicast_dealloc(void);

int multicast_send(char* msg);
int multicast_recv(char* group, int domain, int port, 
                   char* buf, size_t buf_len);
#define DEF_RECV(buf, len) multicast_recv(0, 0, 0, buf, len)

void* convert_addr(struct sockaddr*);

//
// Client - Server communication protocol prototypes
//
//

typedef enum { 
    DATA_TYPE_string, 
    DATA_TYPE_img,
    DATA_TYPE_url, 
    DATA_TYPE_register, 

    DATA_TYPE_COUNT,
} data_type_e;

#define TARGET_MAX 30
#define NO_TARGET "N0_T"
#define TARGET_SYMBOL "<)"

#define MOD_NAME_MAX 124
#define MOD_SYMBOL "(>"
#define RESUME_JOB_SYM "J#"

struct client_req {
    uint32_t  size; // Size of incomming data string
    data_type_e data_type;
    char target[TARGET_MAX];
    char module[MOD_NAME_MAX];
};

#define MSGLEN_MAX 256
struct server_res {
    uint32_t retcode;
    data_type_e data_type;
    uint32_t size;
    char msg[MSGLEN_MAX];
};

ssize_t recvall(int socket, void* buf, ssize_t bytes, int flags);
ssize_t sendall(int socket, void* msg, ssize_t bytes, int flags);
int     send_errmsg(int fd, char* msg);
int     send_errno(int fd);

struct client_req str_to_request(char* str, char* buf, size_t size);
size_t            get_data(char* str, char* buf, size_t size);

#endif
