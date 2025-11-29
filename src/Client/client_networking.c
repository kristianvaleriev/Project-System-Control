#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <asm-generic/socket.h>
#include <bits/getopt_core.h>
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

#include "client_networking.h"

static struct addrinfo saved_addr;

int get_connected_socket(void* addr, size_t size)
{
    struct addrinfo *result, *temp_ai;
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints);
    hints.ai_family    = AF_UNSPEC;
    hints.ai_socktype  = SOCK_STREAM;

    int status = getaddrinfo((char*) addr, KNOWN_PORT, &hints, &result);
    if (status) {
        errno = status;
        return ERR_GETADDRINFO;
    }

    int socket_ret;
    for (temp_ai = result; temp_ai; temp_ai = temp_ai->ai_next) 
    {
        socket_ret = socket(temp_ai->ai_family, SOCK_STREAM, temp_ai->ai_protocol);
        if (socket_ret >= 0) 
        {
            if (!connect(socket_ret, temp_ai->ai_addr, temp_ai->ai_addrlen)) {
                saved_addr = *temp_ai;

                saved_addr.ai_addr  = malloc(sizeof *saved_addr.ai_addr);
                *saved_addr.ai_addr = *temp_ai->ai_addr;

                break;
            }
            else socket_ret = ERR_CONNECT;
        }
        else socket_ret = ERR_SOCKET;
    }
    freeaddrinfo(result);

    return socket_ret;
}

int new_connected_server_socket(void)
{
    int ret = socket(saved_addr.ai_family, SOCK_STREAM, saved_addr.ai_protocol);
    if (ret < 0)
        return ERR_SOCKET;

    if (connect(ret, saved_addr.ai_addr, saved_addr.ai_addrlen))
        return ERR_CONNECT;

    return ret;
}

int validate_ip_address(char *ptr)
{
    struct sockaddr_storage temp;
    if (inet_pton(AF_INET , ptr, &temp) ||
        inet_pton(AF_INET6, ptr, &temp))
        return 1;

    return 0;
}
