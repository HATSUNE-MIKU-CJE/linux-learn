#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    char buf[32];
    int fd = open("test.txt",O_RDONLY);

    //先正常读一次
    ssize_t n =read(fd,buf,5);
    buf[n]='\0';
    printf("Print a bytes: %s\n",buf);

    //把鼠标移回开头，重新读
    lseek(fd,0,SEEK_SET);
    n = read(fd,buf,5);
    buf[n]='\0';
    printf("After rewind, first 5: %s\n",buf);

    //获取当前光标位置
    off_t pos = lseek(fd,0,SEEK_CUR);
    printf("Current position: %ld\n",pos);

    //获取文件总大小
    off_t size = lseek(fd,0,SEEK_END);
    printf("File size: %ld bytes\n",size);

    close(fd);
    return 0;
}