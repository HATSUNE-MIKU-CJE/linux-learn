#include <stdio.h>
#include <unistd.h>

int main()
{
    printf("Before fork, PID = %d\n",getpid());

    pid_t pid = fork();

    if (pid == 0){
        //子进程
        
        printf("[Child]  my PID = %d\n",getpid());
    }
    else if (pid >0)
    {
        //父进程
        usleep(100000);
        printf("[Parent]  child PID = %d, my PID = %d\n",pid,getpid());
    }
    else
    {
        perror("fork failed");
        return 1;
    }

    return 0;
}