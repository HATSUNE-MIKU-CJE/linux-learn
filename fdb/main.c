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
        return 1;
   }

   fdb_t *db = fdb_open(argv[1]);
   if (db == NULL)
   {
        return 1;
   }

   if (strcmp(argv[2],"set")==0)
   {
        if (argc !=5)
        {
            printf("fdb <dbfile> set <key> <value>\n");
            return 1;
        }
        fdb_set(db,argv[3],argv[4]);
   }
   else if (strcmp(argv[2],"get")==0)
   {
        char value[256];
        if (argc != 4)
        {
            printf("fdb <dbfile> get <key>\n");
            return 1;
        }
        fdb_get(db,argv[3],value,sizeof(value));
   }
   else if (strcmp(argv[2],"delete")==0)
   {
        if (argc != 4)
        {
            printf("fdb <dbfile> delete <key>\n");
            return 1;
        }
        fdb_delete(db,argv[3]);
   }
   else if (strcmp(argv[2],"list")==0)
   {
        if (argc != 3)
        {
            printf("fdb <dbfile> list\n");
            return 1;
        }
        fdb_list(db);
   }
   else if (strcmp(argv[2],"compact")==0)
   {
        if (argc != 3)
        {
            printf("fdb <dbfile> compact\n");
            return 1;
        }
        fdb_compact(db);
   }
   else
   {
        printf("Here are commands:\n");
        printf("fdb <dbfile> set <key> <value>\n");
        printf("fdb <dbfile> get <key>\n");
        printf("fdb <dbfile> delete <key>\n");
        printf("fdb <dbfile> list\n");
        printf("fdb <dbfile> compact\n");
   }
   fdb_close(db);
   return 0;
}