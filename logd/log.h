#ifndef LOG_H
#define LOG_H

#include <unistd.h>
#include <limits.h> 

#define MAX_ROTATE 5
#define LOG_MAX_ROTATE 5

typedef struct 
{
    int fd;
    char path[PATH_MAX];
    off_t current_size;
    off_t max_size;
}log_t;

log_t *log_open(const char *path, off_t max_size);
void   log_close(log_t *log);

int    log_write(log_t *log, const char *msg);

#endif
