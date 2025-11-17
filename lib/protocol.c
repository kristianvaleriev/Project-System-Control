#include "../include/network.h"
#include <errno.h>

ssize_t recvall(int socket, void* buf, ssize_t bytes, int flags)
{
    ssize_t nread = 0, total_read = 0;
    for (; total_read < bytes; total_read += nread) 
    {
        buf += total_read;
        if ((nread = recv(socket, buf, bytes - total_read, flags)) < 0) {
            if (total_read)
                break;
            else 
                return -1;
        } 
        else if (!nread) 
            break;
    }
    return total_read;
}

ssize_t sendall(int socket, void* msg, ssize_t bytes, int flags)
{
    ssize_t nwritten, total_send = 0;

    for (; total_send < bytes; total_send += nwritten) 
    {
        msg += total_send;
        if ((nwritten = send(socket, msg, bytes - total_send, flags)) < 0) {
            if (total_send)
                break;
            else 
                return -1;
        }
    }

    return total_send;
}

int send_errmsg(int fd, char* msg) 
{
    struct server_res res;
    memset(&res, 0, sizeof res);
    res.retcode = htonl(-1);
    memmove(res.msg, msg, MSGLEN_MAX);

    if (sendall(fd, &res, sizeof res, 0) != sizeof res) 
        return -1;
    return 0;
}

int send_errno(int fd) 
{
    struct server_res res;
    memset(&res, 0, sizeof res);
    res.retcode = htonl(errno);
    strncpy(res.msg, strerror(errno), sizeof res.msg);

    if (sendall(fd, &res, sizeof res, 0) != sizeof res)
        return -1;
    return 0;
}

size_t get_target(char* str, char* buf, size_t size) 
{
    memset(buf, 0, size);
    char* ptr = 0;
    if ((ptr = strstr(str, TARGET_SYMBOL)))
    {
        size_t len = ptr - str - 1;
        while (isspace(str[len])) 
            len--;
        len++; // from index to length

        size_t start = 0;
        while (isspace(str[start]))
            start++;
        len -= start;
        
        memmove(buf, str + start, len);
        buf[len++] = '\0';
        printf("get_target:: len = %zu, buf = |%s|\n", len, buf);
        return len;
    }
    memmove(buf, NO_TARGET, sizeof NO_TARGET);
    return sizeof NO_TARGET;
}

size_t get_module(char* str, char* buf, size_t size)
{
    memset(buf, 0, size);
    char* ptr = 0;
    if ((ptr = strstr(str, MOD_SYMBOL)))
    {
        ptr += sizeof MOD_SYMBOL;
        while (isspace(*ptr))
            ptr++;
        size_t len = strlen(ptr) - 1;
        while (isspace(ptr[len])) 
            len--;
        len++; // from index to length 
        
        memmove(buf, ptr, len);
        buf[len++] = '\0';
        printf("get_module:: len = %zu, buf = |%s|\n", len, buf);
        return len;
    }
    
    return 0;
}

size_t get_data(char* str, char* buf, size_t size) 
{
    memset(buf, 0, size);
    size_t start = 0; 
    size_t len = strlen(str) - 1;

    char* ptr = 0;
    if ((ptr = strstr(str, TARGET_SYMBOL)))
    {
        start = ptr - str + sizeof(TARGET_SYMBOL);
        while (isspace(str[start]))
            start++;
    }

    if ((ptr = strstr(str, MOD_SYMBOL))) 
        len = ptr - str - 1;

    while (isspace(str[len]))
        len--;
    len++;
    len -= start;

    if (len + 1 > size) {
        memmove(buf, str + start, size);
        buf[size] = '\0';
    } 
    else if (len) {
        memmove(buf, str + start, len);
        buf[len++] = '\0';
    }
    printf("get_data:: start: %zu, len = %zu, buf = |%s|\n", start, len, buf);
    return len;
}

struct client_req str_to_request(char* str, char* buf, size_t size)
{
    struct client_req req;
    memset(&req, 0, sizeof req);

    char target[256];
    size_t target_len = get_target(str, target, sizeof target);
    memmove(req.target, target, target_len);
    
    char module[256];
    size_t module_len = get_module(str, module, sizeof module);
    memmove(req.module, module, module_len);
    
    size_t data_len = get_data(str, buf, size);
    req.size = data_len;

    return req;
}

