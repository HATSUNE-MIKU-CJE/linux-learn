#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main()
{
    printf("Before fork, PID = %d\n",getpid());
    pid_t pid;

    pid = fork();

    if (pid==0)
    {
        printf("Child my PID = %d, PPID = %d\n",getpid(),getppid());
        sleep(2);
        printf("Child my PID = %d, PPID = %d\n",getpid(),getppid());
    }
    else if (pid > 0)
    {
        printf("Parent PID = %d, exiting\n",getpid());
        return 0;
    }
    else
    {
        perror("fork failed");
        return 1;
    }
}