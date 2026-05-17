#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main()
{
    // 打开一个文件，如果不存在就创建，存在就清空
    int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open failed");
        return 1;
    }

    printf("Got fd = %d\n", fd);  // 猜一下会打印什么数字？

    const char *msg = "Hello, file!\n";
    write(fd, msg, strlen(msg));

    int fd2 = open("test2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    printf("Got fd2 = %d\n", fd2);
    close(fd2);

    close(fd);  // 用完了要关，像 STM32 上关外设时钟一样
    return 0;
}
