#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

//
// Syslog error functions:
//
void LOG_SYSCALL(int rc, char* fmt, ...);
void LOG_NTC0(char* fmt, ...);
void LOG_NTC1(char* fmt, ...);

//
// Error wrappers:
//
char* err_malloc(size_t size);
char* err_realloc(char* ptr, size_t size);

//
// Daemon functions
//
void log_print(int errno_flag, int priority, char* msg, va_list args);
void daemonize(void);
int  lock_daemon(void);

