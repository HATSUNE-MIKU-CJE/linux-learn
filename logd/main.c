#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "log.h" 

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t need_rotate = 0;
void sig_handler(int sig)
{
    if (sig == SIGTERM) running = 0;
    if (sig == SIGHUP)  need_rotate = 1;
}

int main(int argc, char *argv[])
{
    char buf[4096];
    if (argc < 4)
    {
        fprintf(stderr,"./logd <fifo路径> <日志文件路径> <大小上限>\n");
        return 1;
    }
    
    if (mkfifo(argv[1],0666)==-1 && errno != EEXIST)
    {
        perror("mkfifo");
        return 1;
    }

    if (fork()!=0)
    {exit(0);}

    setsid();

    if (fork()!=0)
    {exit(0);}

    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO); 

    open("/dev/null",O_RDONLY);
    open("/dev/null",O_WRONLY);
    open("/dev/null",O_WRONLY);

    log_t *log = log_open(argv[2],(off_t)atol(argv[3]));
    if (log == NULL)
    {
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM,&sa,NULL);
    sigaction(SIGHUP,&sa,NULL);

    int fd = open(argv[1],O_RDONLY);
    if (fd==-1)
    {
        return 1;
    }
    while (running)
    {
        ssize_t n = read(fd,buf,sizeof(buf)-1);
        if (n>0)
        {
            buf[n]='\0';
            char *nl = strchr(buf,'\n');
            if (nl) 
            {*nl = '\0';}
            if (need_rotate)
            {
                log->current_size = log->max_size ;
                need_rotate = 0;
                
            }
            if (log_write(log,buf)==-1)
            {
                fprintf(stderr,"log_write failed\n");
            }
            
        }
        else if (n==0)
        {
            close (fd);
            fd = open(argv[1],O_RDONLY);
        }
    }
    unlink(argv[1]);
    close (fd);
    log_close(log);
    return 0;
}