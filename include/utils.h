#ifndef _UTILS_H_
#define _UTILS_H_

#include <sys/types.h>

/*
* Error handling
*/ 

int set_ncurse_err_mode(int);

void err_quit_msg(const char *fmt, ...);
void err_quit_msg(const char *fmt, ...);
void err_dump_abort(const char *fmt, ...);
void err_cont(int error, const char *fmt, ...);
void err_exit(int error, const char *fmt, ...);
void err_sys(const char* fmt, ...);
void err_info(const char *fmt, ...);
void info_msg(const char* fmt, ...);

/*
* Utils
*/ 

void  init_syslog(char*, int, int);
char* set_program_name(char const* argv);
void  set_program_storage(const char* prefix);

ssize_t accumulative_read(int fd, char* buf, size_t buf_size, int timeout, int tries);
ssize_t writeall(int fd, void* buf, size_t n);
char* make_directory(const char*, const char*, const char*);
void set_nonblocking(int fd);

#define create_reading_thread(thd, func, read_fd, write_fd)           \
                   pthread_create(&thd, NULL, func, \
                   (int[]) {read_fd, write_fd})



#endif
