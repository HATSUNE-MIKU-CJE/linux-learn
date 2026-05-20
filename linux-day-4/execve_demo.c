#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    pid_t pid;
    pid = fork();
    char *args[]={"ls","-l",NULL};
    extern char **environ;
    if (pid==0)
    {
        execve("/bin/ls",args,environ);
        perror("execve failed");
        return 1;
    }
    else if (pid > 0)
    {
        wait(NULL);
        printf("child done\n");
    }
    
}