#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

int main()
{
    char name[515];
    char input[512];
    printf("Please type your document:");
    if (scanf("%511s",input)!=1)
    {
        fprintf(stderr,"Input error\n");
        return 1;
    }
    snprintf(name,sizeof(name),"../%s",input);
    DIR *d=opendir(name);
    if (d == NULL)
    {
        perror("opendir");
        return 1;
    }

    struct dirent *e;
    struct stat st;
    char path[772];
    unsigned num;
    char *ch[]={"---","---","---"};
    const char *rwx[]={"---","--x","-w-","-wx","r--","r-x","rw-","rwx"};
    char *t;
    
    while ((e=readdir(d))!=NULL)
    {
        if (strcmp(e->d_name,".")==0 || strcmp(e->d_name,"..")==0) 
        {continue;}
        snprintf(path,sizeof(path),"%s/%s",name,e->d_name);
        if (stat(path,&st)==-1)
        {
            perror("stat");
            continue;
        }
        num=st.st_mode & 0777;
        t=ctime(&st.st_mtime);
        t[24]=0;
        for (int i=0;i<3;i++)
        {
            ch[i]=rwx[(num>>3*(2-i))&7];
        }
        printf("%s%s%s   %-20s %8ld bytes %s  %.16s\n",
               ch[0],
               ch[1],
               ch[2],
               e->d_name,
               st.st_size,
               S_ISDIR(st.st_mode)?"DIR":"FILE",
               t);
    }
    closedir(d);
    return 0;
    
    
}

