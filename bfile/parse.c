#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "bfile.h"


int parse_fields(char *fmt, field_def_t *fields, int max_fields)
{
    char *token;
    char *token2;
    char *save1, *save2;
    int num = 0;
    token = strtok_r(fmt,";",&save1);
    while (token != NULL && num<max_fields)
    {
        token2 = strtok_r(token," ",&save2);
        for (int i=0;i<TYPE_COUNT;i++)
        {
            if (type_table[i].name && strcmp(token2,type_table[i].name)==0)
            {
                fields[num].type = (field_type_t)i;
                break;
            }
        }
        token2 = strtok_r(NULL," ",&save2);
        char *bracket = strchr(token2,'[');
        if (bracket)
        {
            *bracket = '\0';
            fields[num].array_size = atoi(bracket+1);
        }
        else
        {
            fields[num].array_size = 0;
        }
        strncpy(fields[num].name,token2,sizeof(fields[num].name));
        token = strtok_r(NULL,";",&save1);
        num++;
    }
    return num;
}
