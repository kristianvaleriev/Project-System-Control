#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

int get_connected_socket(struct sockaddr* addr)
{
    if (!addr)
        return -1;

    int socket_ret = socket(addr->sa_family, SOCK_STREAM, 0);
    if (socket_ret < 0) 
        return -2;
    
 //   if (connect(socket_ret, addr, ))
}
