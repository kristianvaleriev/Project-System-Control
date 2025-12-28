#ifndef _HANDLE_FILES_H_
#define _HANDLE_FILES_H_

int handle_files(int socket, int term_fd, struct client_request* req);
int handle_drivers(int socket, int term_fd, struct client_request* req);

#endif
