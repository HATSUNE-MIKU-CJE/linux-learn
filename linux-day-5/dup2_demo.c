#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>


int main()
{
    int fd[2];
    
    if (pipe(fd)==-1)
    {
        perror("pipe failed");
        return 1;
    }
    printf("[创建] fd[0]=%d (读端), fd[1]=%d (写端)\n", fd[0], fd[1]);

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        return 1;
    }
    else if (pid == 0)
    {
        close (fd[0]);
        dup2(fd[1],1);
        close(fd[1]);
        execlp("ls","ls","-l",NULL);
    }
    else
    {
        close(fd[1]);
        char buf[4096]={0};
        ssize_t n;
        while((n = read(fd[0],buf,sizeof(buf)-1))>0)
        {
            buf[n]='\0';
            printf("%s",buf);
        }
        close (fd[0]);
        wait(NULL);
        printf("[父进程] 发送完毕，子进程已退出\n");

    }
    return 0;
}