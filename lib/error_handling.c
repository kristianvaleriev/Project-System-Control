#include "../include/includes.h"

#include <stdarg.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/syslog.h>
#include <syslog.h>

/*
* Stolen implemation from the excellent book "Advanced Programming in the UNIX Envi"
* altho it is prety standard anyway (just lazy to make my own ig). 
* Atleast I made them prettier! (Heavaly modified)
*/

#define MAXLINE 4096
#define NAME_DISPLAY "%s:: "

extern char* program_name;

// Server specific logic
static int is_daemon;
int priority = LOG_NOTICE;

void init_syslog(char* name, int option, int facility)
{
    openlog(name, option, facility);
    is_daemon = 1;
}


static void err_doit(int errnoflag, int error, const char *fmt, va_list ap)
{
    int err_copy = errno;

    char buf[MAXLINE];
    size_t len = 0;

    if (program_name)
        len = snprintf(buf, MAXLINE-1, NAME_DISPLAY, program_name);
    len += vsnprintf(buf + len, MAXLINE-len-1, fmt, ap);

    if (errnoflag && error)
        snprintf(buf+len, MAXLINE-len-1, ": %s",
                 strerror(error));

    strcat(buf, "\n\r");

    if (!is_daemon) {
        fflush(stdout); /* in case stdout and stderr are the same */
        fputs(buf, stderr);
        fflush(NULL); /* flushes all stdio output streams */
    }
    else 
        syslog(priority, "%s", buf);

    errno = err_copy;
}

/*
* Fatal error unrelated to a system call.
* Print a message and terminate.
*/
void err_quit_msg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if (is_daemon)
        priority = LOG_CRIT;

    err_doit(0, 0, fmt, ap);

    va_end(ap);
    exit(1);
}

void info_msg(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if (is_daemon)
        priority = LOG_INFO;

    err_doit(0,0, fmt, ap);
    va_end(ap);
}

/*
* Fatal error related to a system call.
* Print a message, dump core, and terminate.
*/
void err_dump_abort(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (is_daemon)
        priority = LOG_ERR;

    err_doit(1, errno, fmt, ap);

    va_end(ap);
    abort(); /* dump core and terminate */
    exit(1); /* shouldn’t get here */
}

/*
* Nonfatal error unrelated to a system call.
* Error code passed as explict parameter.
* Print a message and return.
*/
void err_cont(int error, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if (is_daemon)
        priority = LOG_WARNING;

    err_doit(1, error, fmt, ap);
    va_end(ap);
}
/*
* Fatal error unrelated to a system call.
* Error code passed as explict parameter.
* Print a message and terminate.
*/
void err_exit(int error, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (is_daemon)
        priority = LOG_CRIT;

    err_doit(1, error, fmt, ap);
    va_end(ap);
    exit(1);
}

/*
* Fatal error related to a system call.
* Print a message and terminate.
* (Most useful!)
*/
void err_sys(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (is_daemon)
        priority = LOG_ERR;

    err_doit(1, errno, fmt, ap);

    va_end(ap);
    exit(1);      
}

/*
* Nonfatal error related to a system call.
* Print a message and return.
*/
void err_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (is_daemon)
        priority = LOG_WARNING;

    err_doit(1, errno, fmt, ap);
    va_end(ap);
}
