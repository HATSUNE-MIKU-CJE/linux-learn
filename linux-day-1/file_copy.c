#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    char buf[64];

    int src_fd = open("test.txt",O_RDONLY);
    int dst_fd = open("test.copy.txt",O_WRONLY | O_CREAT | O_TRUNC, 0644);

    ssize_t n;
    while ((n=read(src_fd,buf,sizeof(buf))) > 0)
    {
        write(dst_fd,buf,n);
    }
    close(src_fd);
    close(dst_fd);
    return 0;
}