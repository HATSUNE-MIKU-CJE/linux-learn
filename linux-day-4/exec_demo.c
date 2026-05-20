#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    pid_t pid;
    int child_num;
    int status;
    pid_t result;
    for (int i=0;i<3;i++)
    {
        pid = fork();
        if (pid == 0)
        {
            child_num=i;
            break;
        }

    }

    if (pid==0)
    {
        sleep(child_num);
        switch (child_num)
        {
        case 0:
            execlp("ls","ls","-l",NULL);
            break;
        case 1:
            execlp("cat","cat","exec_demo.c",NULL);
            break;
        case 2:
            execlp("whoami","whoami",NULL);
            break;
        }
        perror("execlp failed");
        return 1;
    }
    else if (pid > 0){
        int reaped = 0;
        while(reaped<3)
        {
            result=waitpid(-1, &status, WNOHANG);
            if (result==0)
            {
                printf("No child yet, doing other work...\n");
                sleep(1);
            }
            else if (result >0)
            {
                printf("Child PID = %d done\n",result);
                reaped++;
            } 
        }
    }
    return 0;
}