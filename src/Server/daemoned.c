#include "../../include/includes.h"
#include "../../include/utils.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "daemoned.h"

/*
* Old code again, sorry... Same comment as in 
* the multicast functions in netlib.c
*/

extern char* program_name;

static DIR* check_dir(char* dirname, char* path) 
{
	char pathname[256];
    char* fmt = path[strlen(path)-1] == '/' ? "%s%s" : "%s/%s";
    sprintf(pathname, fmt, path, dirname);
	
	if (mkdir(pathname, 0755) < 0) 
		if (errno != EEXIST) 
            return 0;

	DIR* ret = opendir(pathname);
	return ret;
}

static int open_lockdir(void)
{
	char path_fmt[256] = "/var/run/user/";
	strcat(path_fmt, "%u/");

	char path[256];
	snprintf(path, sizeof path, path_fmt, geteuid());
    
    extern char* program_name;
	return dirfd(check_dir(program_name, path));
}

void daemonize(void)
{
	umask(0);

	int pid = fork();
	if (pid < 0) 
        err_sys("fork");
	if (pid) exit(0);

	setsid();

	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags	  = 0;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGHUP, &sa, 0) < 0)
        err_sys("sigaction");

	if (chdir("/") < 0)
        err_sys("can't change working dir to root dir (/)");

	struct rlimit rlim;
	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0)
        err_sys("can't get file limit");
	
	if (rlim.rlim_max == RLIM_INFINITY) 
		rlim.rlim_max =  1024;
	for (size_t i = 0; i < rlim.rlim_max; i++) 
        close(i);
    errno = 0;

	int fd0 = open("/dev/null", O_RDWR);
	int fd1 = dup(fd0);
	int fd2 = dup(fd0);

	if (fd0 != 0 || fd1 != 1 || fd2 != 2)
        err_quit_msg("strange file descriptors");

    if (lock_daemon() == 1)
        err_quit_msg("daemoned already running");
}

int lock_daemon(void)
{
	int lock_dir = open_lockdir();
	if (lock_dir < 0)
        err_sys("can't open user lock directory");

	int fd = openat(lock_dir, "lock.pid", O_RDWR | O_CREAT, 0644);
	if (fd < 0)
        err_sys("can't open lock file");
	
	struct flock fl;
	fl.l_whence = SEEK_SET;
	fl.l_start  = 0;
	fl.l_len 	= 0;
	fl.l_type   = F_WRLCK;

    int ret = 0;

	if (fcntl(fd, F_SETLK, &fl) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			ret = 1;
			goto RET;
		}
		err_sys("fctnl");
	}

	ftruncate(fd, 0);
	char buf[16] = {0};
	sprintf(buf, "%ld", (long) getpid());
	write(fd, buf, strlen(buf) + 1);

  RET: 
    close(fd);
    close(lock_dir);

    return ret;
}

