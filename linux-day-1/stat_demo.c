#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

int main()
{
    struct stat st;

    if (stat("test.txt",&st)==-1)
    {
        perror("stat failed");
        return 1;
    }


    printf("Size: %ld bytes\n",st.st_size);
    //printf("Mode(raw): %o (octal)\n",st.st_mode);
    //printf("Mode(per): %o (octal)\n",st.st_mode & 0777);
    printf("Mode: %o (octal)\n",st.st_mode & 0777);
    printf("UID: %d\n",st.st_uid);
    printf("Links: %lu\n",st.st_nlink);

    if (S_ISREG(st.st_mode))
    {
        printf("Type: regular file\n");
    }
    else if (S_ISDIR(st.st_mode))
    {
        printf("Type: directory\n");

    }
    else if (S_ISCHR(st.st_mode))
    {
        printf("Type: char\n");
    }
    return 0;
    
}