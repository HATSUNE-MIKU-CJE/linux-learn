/* main.c — CLI 入口
 * 纯命令行解析，不包含任何业务逻辑
 *
 * 用法:
 *   fdb <dbfile> set <key> <value>
 *   fdb <dbfile> get <key>
 *   fdb <dbfile> delete <key>
 *   fdb <dbfile> list
 *   fdb <dbfile> compact
 *
 * TODO: 实现参数解析 + 调用对应 fdb_xxx()
 */

#include <stdio.h>
#include <string.h>
#include "fdb.h"

int main(int argc, char *argv[])
{
    if (argc<3)
    {
         fprintf(stderr,"Here are commands:\n");
         fprintf(stderr,"fdb <dbfile> set <key> <value>\n");
         fprintf(stderr,"fdb <dbfile> get <key>\n");
         fprintf(stderr,"fdb <dbfile> delete <key>\n");
         fprintf(stderr,"fdb <dbfile> list\n");
         fprintf(stderr,"fdb <dbfile> compact\n");
         return 1;
    }

    fdb_t *db = fdb_open(argv[1]);
    if (db == NULL)
    {    
         fprintf(stderr, "fdb: cannot open %s\n", argv[1]);
         return 1;
    }

    if (strcmp(argv[2],"set")==0)
    {
         if (argc !=5)
         {
              fprintf(stderr,"fdb <dbfile> set <key> <value>\n");
              return 1;
         }
         if (fdb_set(db,argv[3],argv[4])==-1)
         {
              fprintf(stderr, "set: failed to write '%s'\n", argv[3]);

              return 1;
         }
    }
    else if (strcmp(argv[2],"get")==0)
    {
         char value[256];
         if (argc != 4)
         {
              fprintf(stderr,"fdb <dbfile> get <key>\n");
              return 1;
         }
         if (fdb_get(db,argv[3],value,sizeof(value))==-1)
         {
              fprintf(stderr,"get: key not found: %s\n",argv[3]);
              return 1;
         }
         printf("%s\n",value);
    }
    else if (strcmp(argv[2],"delete")==0)
    {
         if (argc != 4)
         {
              fprintf(stderr,"fdb <dbfile> delete <key>\n");
              return 1;
         }
         if (fdb_delete(db,argv[3])==-1)
         {
              fprintf(stderr, "delete: failed to remove '%s'\n", argv[3]);

              return 1;
         }
    }
    else if (strcmp(argv[2],"list")==0)
    {
         if (argc != 3)
         {
              fprintf(stderr,"fdb <dbfile> list\n");
              return 1;
         }
         if (fdb_list(db)==-1)
         {
              fprintf(stderr, "list: failed\n");

              return 1;
         }
    }
    else if (strcmp(argv[2],"compact")==0)
    {
         if (argc != 3)
         {
              fprintf(stderr,"fdb <dbfile> compact\n");
              return 1;
         }
         if (fdb_compact(db)==-1)
         {
              fprintf(stderr, "compact: failed\n");

              return 1;
         }
    }
    else
    {
         fprintf(stderr,"Here are commands:\n");
         fprintf(stderr,"fdb <dbfile> set <key> <value>\n");
         fprintf(stderr,"fdb <dbfile> get <key>\n");
         fprintf(stderr,"fdb <dbfile> delete <key>\n");
         fprintf(stderr,"fdb <dbfile> list\n");
         fprintf(stderr,"fdb <dbfile> compact\n");
    }
    fdb_close(db);
    return 0;
}