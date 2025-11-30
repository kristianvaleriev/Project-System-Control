#ifndef _ACTION_ARRAY_H_
#define _ACTION_ARRAY_H_

#include "../../include/includes.h"
#include "../../include/network.h"

#include "server_pty.h"
#include "file_handling.h"

typedef void (*action_function)(int socket, int term_fd,
                                struct client_request*);

static action_function action_array[] = {
    [TYPE_WINSIZE] = set_win_size,
    [TYPE_DRIVERS] = handle_drivers,
    [TYPE_FILES]   = handle_files,
};

#endif
