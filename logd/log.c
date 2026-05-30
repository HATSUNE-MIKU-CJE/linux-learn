#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "log.h"

log_t *log_open(const char *path, off_t max_size)
{
    int fd = open(path,O_WRONLY | O_CREAT | O_APPEND,0644); 
    if (fd ==-1)
    {
        return NULL;
    }
    log_t *log = malloc(sizeof(log_t));
    if (log == NULL)
    {
        close(fd);
        return NULL;
    }
    log->fd = fd;
    strncpy(log->path,path,sizeof(log->path));
    log -> path[PATH_MAX-1] = '\0';
    log->max_size = max_size;
    log->current_size = lseek(fd,0,SEEK_END);
    return log;
}
void   log_close(log_t *log)
{
    close (log->fd);
    free(log);
}
int    log_write(log_t *log, const char *msg)
{
    char line[4096];
    time_t rawtime;
    struct tm *info;
    char buffer[80];

    time(&rawtime);
    info = localtime(&rawtime);
    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);

    snprintf(line,sizeof(line),"[%s] %s\n",buffer,msg);
    size_t line_len  = strlen(line);
  
    if (log->current_size + line_len >= log->max_size)
    {
        char old_path[PATH_MAX],new_path[PATH_MAX];
        close(log->fd);
        for (int i=LOG_MAX_ROTATE-1;i>=1;i--)
        {
            snprintf(old_path,PATH_MAX,"%s.%d",log->path,i);
            snprintf(new_path,PATH_MAX,"%s.%d",log->path,i+1);
            rename(old_path,new_path);
        }
        snprintf(new_path,PATH_MAX,"%s.1",log->path);
        if (rename(log->path,new_path)==-1)
        {
            perror("Rename failed");
            return -1;
        }
        
        log->fd = open(log->path,O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log->fd == -1)
        {
            perror("open failed");
            return -1;
        }  
        log->current_size = 0;
    }

    if (write(log->fd,line,line_len)==-1)
    {
        perror("write failed");
        return -1;
    }
    log->current_size += line_len;
    return 0;
}