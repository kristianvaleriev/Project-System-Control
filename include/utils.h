#ifndef _UTILS_H_
#define _UTILS_H_

/*
* Error handling
*/ 

void err_quit_msg(const char *fmt, ...);
void err_quit_msg(const char *fmt, ...);
void info_msg(const char* fmt, ...);
void err_dump_abort(const char *fmt, ...);
void err_cont(int error, const char *fmt, ...);
void err_exit(int error, const char *fmt, ...);
void err_sys(const char* fmt, ...);
void err_info(const char *fmt, ...);

/*
* Utils
*/ 

void  init_syslog(char*, int, int);
char* set_program_name(char const* argv);

#endif
