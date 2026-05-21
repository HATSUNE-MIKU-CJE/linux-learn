#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(void)
{
    int fd[2];  // fd[0]=读端, fd[1]=写端

    // 步骤1: 创建管道。调完这一行，fd[0]和fd[1]就是有效的文件描述符了
    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }
    printf("[创建] fd[0]=%d (读端), fd[1]=%d (写端)\n", fd[0], fd[1]);

    // 步骤2: fork
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // ========== 子进程：读数据 ==========
        close(fd[1]);                       // 我不写，关掉写端

        char buf[128] = {0};
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);  // 阻塞等数据 
        close(fd[0]);                       // 读完关掉读端

        printf("[子进程] 读到 %zd 字节: %s", n, buf);

    } else {
        // ========== 父进程：写数据 ==========
        close(fd[0]);                       // 我不读，关掉读端

        const char *msg = "hello from parent\n";
        write(fd[1], msg, strlen(msg));     // 写入管道

        close(fd[1]);                       // 写完关写端 → 子进程read收到EOF
        wait(NULL);                         // 等子进程结束
        printf("[父进程] 发送完毕，子进程已退出\n");
    }

    return 0;
}
