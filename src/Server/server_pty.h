#ifndef _PTY_H_
#define _PTY_H_

void set_control_terminal(int term_fd, char* path);
int set_win_size(int client_socket, int term_fd, struct client_request* req);

#endif
