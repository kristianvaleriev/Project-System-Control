#ifndef _CLIENTHANDLING_H_
#define _CLIENTHANDLING_H_

#include <unistd.h>

void  handle_client(int client_socket);
pid_t handle_child_exec(int* fd, 
                        int (*)(int));

#endif
