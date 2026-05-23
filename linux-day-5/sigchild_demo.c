#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>


int alive = 3;

void sigchild_handler(int sig)
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1,&status,WNOHANG))>0)
    {
        printf("[Parent] find [Child] PID = %d, exit = %d\n",pid,WEXITSTATUS(status));
        kill(getpid(),SIGUSR1);
        alive--;
    }
}

void write_handler(int sig)
{
    printf("hhhh,find a child...\n");
}

int main()
{
    pid_t pid;
    int child_num;
    struct sigaction sa;
    struct sigaction sb;
    sa.sa_handler = sigchild_handler;
    sa.sa_flags = SA_RESTART;
    sb.sa_handler = write_handler;
    sb.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1,&sb,NULL);
    sigaction(SIGCHLD,&sa,NULL);
    for (int i=0;i<3;i++)
    {
        pid = fork();
        if (pid == 0)
        {
            child_num = i;
            break;
        }
    }
    if (pid < 0)
    {
        perror("fork failed");
        return 1;
    }
    else if (pid == 0)
    {
        sleep(child_num);
        return child_num;
    }
    else
    {
        while(alive > 0)
        {
            pause();
        }
       
    }
    return 0;
}