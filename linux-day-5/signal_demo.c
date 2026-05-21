#include <stdio.h>
#include <signal.h>

int count = 0;

void handler(int sig)
{
    count++;
    printf("按了第 %d 次 Ctrl+C",count);
}

int main()
{
    signal(SIGINT,handler);
    while (1)
    {

        if (count==3)
        {
            break;
        }
    }
    return 0;
}