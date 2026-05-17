#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    char buf[64];

    int fd = open("test.txt",O_RDONLY);
    if (fd<0)
    {
        perror("open failed");
        return 1;
    }

    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read failed");
        return 1;
    }
    buf[n] = '\0';

    printf("Read %ld bytes: %s", n, buf);
    close(fd);
    return 0;
}    
