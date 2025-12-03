#include "../../include/includes.h"
#include "../../include/network.h"
#include "../../include/utils.h"

#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include <linux/module.h>
#include <sys/syscall.h>

#include <sys/wait.h>

#define ACCESS_MODE   0744

static void flush_recv_out(int socket, size_t size)
{
    char* buf = malloc(size);
    size_t rc;
    while ((rc = recv(socket, buf, size, 0)) > 0) {
        if (rc >= size)
            break;
        size -= rc;
    }

    free(buf);
}

static void create_file_path(char* pathname)
{
    char* buf = calloc(MAX_FILENAME, 1);
    if (!buf)
        return; 

    size_t len = strlen(pathname);
    size_t offset = 0;

    for (size_t i = 0; i < len; i++) 
    {
        if (pathname[i] != '/')
            continue;

        memcpy(buf + offset, pathname + offset, i - offset);
        offset = i;

        mkdir(buf, ACCESS_MODE);
    }

    free(buf);
    return;
}

static void write_file(int socket, int fd, size_t size)
{
    size_t rc;
    char* buf = calloc(size, 1);

    while ((rc = recv(socket, buf, size, 0)) > 0) {
        if (write(fd, buf, rc) < 0) 
            err_info("write fail");
        
        if (rc >= size)
            break;
        size -= rc;
    }
    free(buf);

    if (rc < 0)
        err_info("recv() in write to file");
}

static int create_file(int socket, char* filename, size_t filesize)
{
    if (strstr(filename, "/"))
        create_file_path(filename);

    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, ACCESS_MODE);
    if (fd < 0) {
        err_info("could not create file '%s'", filename);
        flush_recv_out(socket, filesize);
        return -1;
    }

    write_file(socket, fd, filesize);

    close(fd);

    info_msg("file '%s' successfully stored", filename);
    return 0;
}

static ssize_t recv_filename(int socket, char* filename, size_t size)
{
    ssize_t rc = recv(socket, filename, MAX_FILENAME, 0);
    if (rc < 0) {
        err_info("recv on filename");
        flush_recv_out(socket, size);
    }

    return rc;
}

void handle_files(int socket, int term_fd, struct client_request* req)
{
    char filename[MAX_FILENAME] = {0};
    if (recv_filename(socket, filename, MAX_FILENAME) <= 0) 
        return;

    create_file(socket, filename, req->data_size);
}


static char*  set_working_dir(char* filename);
static void   restore_working_dir(void);
static pid_t    compile_driver(char* driver_name, char* cwd);
static void   install_driver(char* driver_name, size_t len);

void handle_drivers(int socket, int term_fd, struct client_request* req)
{
    char filename[MAX_FILENAME] = {0};
    if (recv_filename(socket, filename, MAX_FILENAME) <= 0)
        return;

    size_t len;
    char* ptr = strrchr(filename, '/');
    if (ptr) {
        len = strlen(++ptr);
        memmove(filename, ptr, len);
        memset(filename + len, 0, MAX_FILENAME - len);
    }
    else len = strlen(filename);

    char* cwd = set_working_dir(filename);

    if (create_file(socket, filename, req->data_size) < 0) {
        info_msg("driver creation failed");
        return;
    }

    pid_t pid = compile_driver(filename, cwd);
    
    int status;
    while ((status = waitpid(pid, NULL, WNOHANG)) == 0) 
        ;

    if (status < 0)   
        err_sys("waitpid failed");

    install_driver(filename, len);

    free(cwd);

    restore_working_dir();
}

extern char* program_storage;
static char* saved_cwd;

#define MAX_PATH_ALLOC 256

static char* set_working_dir(char* filename)
{
    saved_cwd = calloc(MAX_PATH_ALLOC, 1);
    if (!getcwd(saved_cwd, MAX_PATH_ALLOC)) 
        err_sys("getcwd");

    char* new_wd = make_directory(program_storage, filename, "-mod.d");
    if (chdir(new_wd) < 0) 
        err_sys("chdir on set");

    return new_wd;
}

static void restore_working_dir(void)
{
    if (saved_cwd) 
        return;

    if (chdir(saved_cwd) < 0)
        err_sys("chdir on restore");

    free(saved_cwd);
}


static void create_makefile(char* filename, size_t len)
{

#define OBJM "obj-m += "

    int fd = open("Makefile", O_WRONLY | O_CREAT | O_TRUNC, ACCESS_MODE);
    if (fd < 0)
        err_sys("could not create Makefile");

    filename[len - 1] = 'o';

    char buf[MAX_FILENAME + sizeof OBJM] = OBJM;
    strcat(buf, filename);

    if (write(fd, buf, strlen(buf)) < 0)
        err_sys("write to Makefile failed");

    filename[len - 1] = 'c';
}


#define KERNEL_MAKE "/usr/lib/modules/%s/build/Makefile"

static char* get_kernel_build_dir(void)
{
    static char* ret = 0;
    if (ret)
        return ret;

    struct utsname uname_info;
    if (uname(&uname_info) < 0) {
        err_info("could not get OS name and version");
        return NULL;
    }

    size_t size = strlen(uname_info.release) + sizeof KERNEL_MAKE;
    ret = calloc(size, 1);
    snprintf(ret, size, KERNEL_MAKE, uname_info.release);

    return ret;
}

static pid_t compile_driver(char* driver_name, char* cwd)
{
    char* kernel_dir = get_kernel_build_dir();
    if (!kernel_dir)
        err_sys("could not get kernel build directory");

    pid_t pid = fork();
    if (pid < 0)
        err_sys("fork() in compile driver");
    if (pid)
        return pid;


    create_makefile(driver_name, strlen(driver_name));
   
    char arg_M[128] = "M=";
    if (!strcat(arg_M, cwd))
        err_sys("strcat failed");

    char* make_argv[] = {
        "/usr/bin/make",
        "-f", kernel_dir,
        arg_M,
        NULL
    };

    int fd = open("compile.log", O_WRONLY | O_CREAT | O_TRUNC, ACCESS_MODE);

    close(STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd != STDIN_FILENO || fd != STDOUT_FILENO ||
        fd != STDERR_FILENO)
        close(fd);

    execv(make_argv[0], &make_argv[0]);
    info_msg("make failed");

    _exit(-2);
}

static void install_driver(char* driver_name, size_t len)
{
    size_t kobj_len = len + 10;
    char* kobj = calloc(kobj_len, 1);
    memmove(kobj, driver_name, len);

    kobj[len-1] = 'k';
    kobj[len]   = 'o';

    int kobj_fd = open(kobj, O_RDONLY);
    if (kobj_fd < 0) { 
        if (errno == ENOENT)
            info_msg("%s could not compile. Check compile.log", driver_name);
        else 
            err_info("open of %s kobj failed", kobj);
        goto OUT;
    }
    info_msg("%s successfully compiled", kobj);

    if (geteuid()) {
        info_msg("not running as root. Cannot install %s", kobj);
        goto OUT;
    }

    if (syscall(SYS_finit_module, kobj_fd, (char*) {""}, 0
//               MODULE_INIT_IGNORE_MODVERSIONS |
//               MODULE_INIT_IGNORE_VERMAGIC
        ) < 0)
    {
        err_info("syscall of init_module failed (errno: %d)", errno);
    }
    else 
        info_msg("%s driver module successfully installed", kobj);

  OUT:
    free(kobj);
}
