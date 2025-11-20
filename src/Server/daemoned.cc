#include "../../include/includes.h"
#include "../../include/utils.h"

#include "daemon.h"

void daemonize()
{
	umask(0);

	int pid = fork();
	ERR_SYSCALL(pid, "fork");

	if (pid != 0) exit(0);
	setsid();

	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags	  = 0;
	sigemptyset(&sa.sa_mask);
	
	ERR_SYSCALL(sigaction(SIGHUP, &sa, 0), "can't ignore SIGHUP");
	ERR_SYSCALL(chdir("/"), "can't change root dir to /");

	struct rlimit rlim;
	ERR_SYSCALL(getrlimit(RLIMIT_NOFILE, &rlim), "can't get file limit");
	
	openlog(prog_name, LOG_CONS, LOG_DAEMON);

	if (rlim.rlim_max == RLIM_INFINITY) 
		rlim.rlim_max = 1024;
	for (size_t i = 0; i < rlim.rlim_max; i++) 
        close(i);
    errno = 0;

	int fd0 = open("/dev/null", O_RDWR);
	int fd1 = dup(fd0);
	int fd2 = dup(fd0);

	ERR_EXPR(fd0 != 0 || fd1 != 1 || fd2 != 2, "strange file descriptors");
}

static int open_lockdir(void)
{
	char path_fmt[256] = "/var/run/user/";
	strcat(path_fmt, "%u/");

	char path[256];
	snprintf(path, sizeof path, path_fmt, geteuid());

	return dirfd(check_dir(prog_name, path));
}

int lock_daemon(void)
{
	int lock_dir = open_lockdir();
	LOG_SYSCALL(lock_dir, "can't open user lock directory");

	int fd = openat(lock_dir, "lock.pid", O_RDWR | O_CREAT, 0644);
	LOG_SYSCALL(fd, "can't open lock file");
	
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
		LOG_NTC1("fctnl");
		exit(1);
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

