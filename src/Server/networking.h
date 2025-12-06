#ifndef _NETWORKING_H_
#define _NETWORKING_H_

int   get_listening_socket(struct addrinfo* binded_ai);
char* get_host_addr(struct sockaddr* addr, struct addrinfo* hint);

void  set_wait_time(int time);
void* multicast_beacon(void*);

#endif

