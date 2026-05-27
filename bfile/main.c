#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "bfile.h"

int main(int argc,char *argv[])
{
    char *filepath, *fmt;
    char *endian = "little";
    unsigned long offset=0;
    unsigned long offset_current=0;
    int fd,num;
    int big_endian=0;
    uint8_t buf[8];
    field_def_t fields[MAX_FIELDS];
    uint64_t value = 0;
    read_func_t reader;

    for (int i=1;i<argc;i++)
    {
        if (strcmp(argv[i],"-f")==0)
        {
            filepath = argv[++i];
        }
        else if (strcmp(argv[i],"-s")==0)
        {
            fmt = argv[++i];
        }
        else if (strcmp(argv[i],"-e")==0)
        {
            endian = argv[++i];
        }
        else if (strcmp(argv[i],"-o")==0)
        {
            offset = strtoul(argv[++i],NULL,0);
        }
    }
    offset_current = offset;
    num = parse_fields(fmt,fields,MAX_FIELDS);
    if (endian && strcmp(endian,"big")==0)
    {
        big_endian = 1;
    }
    fd = open(filepath,O_RDONLY);
    if (fd==-1)
    {
        fprintf(stderr,"open failed");
        return 1;
    }
    lseek(fd,offset,SEEK_SET);
    for (int j=0;j<num;j++)
    {
        value = 0;
        if (fields[j].array_size == 0)
        {
            read(fd,buf,type_table[fields[j].type].size);
            reader = type_table[fields[j].type].reader;
            
            if (reader==NULL) 
            {printf("0x%04lx  %-10s  %-10s  [not supported]\n", 
            offset_current, type_table[fields[j].type].name,
            fields[j].name);}
            else
            {
                reader(buf,big_endian,&value);
                printf("0x%04lx %-10s %-10s 0x%lx\n",
                offset_current, 
                type_table[fields[j].type].name,fields[j].name, value);
            }
            offset_current+=type_table[fields[j].type].size;
        }
        else
        {
            if (fields[j].type == TYPE_CHAR && fields[j].array_size >0)
            {
                int n = fields[j].array_size;
                char str[n+1];
                read(fd,str,n);
                str[n] = '\0';
                printf("0x%04lx  %-10s  %-10s  \"%s\"\n",
                offset_current, "char", fields[j].name, str);
                offset_current+=n;
                continue;
            }
            for (int k=0;k<fields[j].array_size;k++)
            {
                read(fd,buf,type_table[fields[j].type].size);
                
                reader = type_table[fields[j].type].reader;
                if (reader==NULL) 
                {printf("0x%04lx  %-10s  %-10s  [not supported]\n", 
                offset_current, type_table[fields[j].type].name,
                fields[j].name);}
                else
                {
                    reader(buf,big_endian,&value);
                    printf("0x%04lx %-10s %-10s 0x%lx\n",
                    offset_current, type_table[fields[j].type].name,
                    fields[j].name, value);
                    
                }
                offset_current+=type_table[fields[j].type].size;                
            }
        }
    }
    close(fd);
    return 0;
}
