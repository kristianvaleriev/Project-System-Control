#ifndef _ACTION_ARRAY_H_
#define _ACTION_ARRAY_H_

#include "../../include/includes.h"
#include "../../include/network.h"

#include "server_pty.h"
#include "file_handling.h"
#include "program_req.h"

#define CLIENT_QUIT -13190

pid_t handle_child_exec(int* fd, 
                        int (*)(int));

typedef int (*action_function)(int socket, int term_fd,
                                struct client_request*);

static const action_function action_array[] = {
    [TYPE_WINSIZE] = set_win_size,
    [TYPE_PROGRAM] = handle_program,
    [TYPE_DRIVERS] = handle_drivers,
    [TYPE_FILES]   = handle_files,
};

#endif
