#include "../../include/system.h"
#include "../../include/conf_files.h"
#include "daemon.h"

void log_print(int errno_flag, int priority, char* msg, va_list args)
{
    char err[256] = {0};
    if (errno_flag) 
        snprintf(err, sizeof err, "(%s)", strerror(errno));

    char fmt[1024];
    vsprintf(fmt, msg, args);

    syslog(priority, "%s:: %s %s", prog_name,  fmt, err);
}

void LOG_SYSCALL(int rc, char* fmt, ...)
{
    if (rc >= 0)
        return;

    va_list args;
    va_start(args, fmt);
    log_print(1, LOG_ERR, fmt, args);
    va_end(args);

    exit(1);
}

void LOG_NTC0(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_print(0, LOG_NOTICE, fmt, args);
    va_end(args);
}

void LOG_NTC1(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_print(1, LOG_NOTICE, fmt, args);
    va_end(args);
}

char* err_malloc(size_t size) 
{
    char* ptr = malloc(size);
    if (!ptr) {
        LOG_NTC0("malloc error");
        exit(1);
    }

    return ptr;
}

char* err_realloc(char* ptr, size_t size) 
{
    ptr = realloc(ptr, size);
    if (!ptr) {
        LOG_NTC0("realloc error");
        exit(1);
    }

    return ptr;
}

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

