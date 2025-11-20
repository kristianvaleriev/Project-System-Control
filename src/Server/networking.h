#ifndef _NETWORKING_H_
#define _NETWORKING_H_

int   get_listening_socket(struct addrinfo* binded_ai);
void  print_ip_addr(void* sock_addr, char* msg,
                    void (*print_fn)(const char* fmt, ...));
#endif

