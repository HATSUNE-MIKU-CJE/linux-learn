#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    printf("Before fork, PID = %d\n",getpid());
    pid_t pid;
    int child_num;
    int status;

    for (int i=0;i<5;i++)
    {
        pid = fork();
        if (pid==0)
        {
            child_num = i;
            break;
        }
    }
    if (pid ==0)
    {
        sleep(child_num);
        printf("[Child]  my PID = %d, exiting\n",getpid());
        return child_num;
    }
    else if (pid >0)
    {
        //父进程
        for (int i=0;i<5;i++)
        {
            pid_t dead_child = wait(&status);
            if (WIFEXITED(status)){
                printf("[Parent]  child PID = %d, exited with code %d\n",dead_child,WEXITSTATUS(status));
            }
        }
        printf("All children reaped, exiting\n");
    }
    else
    {
        perror("fork failed");
        return 1;
    }

    return 0;
}