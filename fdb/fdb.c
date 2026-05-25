/* fdb.c — 核心业务层
 * 把 record（磁盘读写）和 index（内存索引）组合成对外API
 *
 * TODO: 实现以下函数
 *   fdb_open()     → open文件 → mmap → 读header → index_build()
 *   fdb_close()    → munmap → close
 *   fdb_set()      → 查重/取空闲槽 → 构造record → atomic_write → 更新索引
 *   fdb_get()      → 查索引 → 读record → 拷贝value
 *   fdb_delete()   → 标记tombstone → 更新索引 → 加入空闲链表
 *   fdb_list()     → 遍历索引，打印所有key
 *   fdb_compact()  → 建新文件，拷贝有效record，rename替换
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "index.h"
#include "record.h"
#include "fdb.h"

fdb_t *fdb_open(const char *path)
{
    int fd = open(path,O_RDWR);
    fdb_header_t hdr;
    index_t *idx;
    if (fd == -1)
    {
        fd = record_create(path,FDB_DEFAULT_RECORD_COUNT);
        if (fd == -1) return NULL;
    }
    fdb_t *db = (fdb_t*)malloc(sizeof(fdb_t));
    if (db == NULL) return NULL;
    db->fd = fd;
    if (record_read_header(fd,&hdr)==-1)
    {return NULL;}
    db->hdr = hdr;
    idx = index_create(FDB_DEFAULT_INDEX_SIZE);
    db->idx = idx;
    index_build(idx,fd,hdr.record_count); 
    strncpy(db->path,path,sizeof(db->path));
    return db;
}

void  fdb_close(fdb_t *db)
{
    index_destroy(db->idx);
    close(db->fd);
    free(db);
}

int   fdb_get(fdb_t *db, const char *key, char *value, size_t len)
{
    int rec_no = index_find(db->idx,key);
    if (rec_no==-1)
    {
        return -1;
    }
    fdb_record_t rec;
    if (record_read(db->fd,rec_no,&rec)==-1)
    {return -1;}
    strncpy(value,rec.value,len);
    value[len-1]='\0';
    return 0;
}
int   fdb_set(fdb_t *db, const char *key, const char *value)
{
    fdb_record_t rec;
    int rec_no = index_find(db->idx,key);
    if (rec_no==-1)
    {
        for (int i=0;i<db->hdr.record_count;i++)
        {
            if (record_read(db->fd,i,&rec)==-1)
            {return -1;}
            if (rec.flags==0||rec.flags==2)
            {
                rec_no = i;
                break;
            }
        }
    }
    if (rec_no==-1)
    {
        rec_no = db->hdr.record_count;
        db->hdr.record_count++;
    }
    memset(&rec,0,sizeof(rec));
    rec.flags=1;
    strncpy(rec.key,key,sizeof(rec.key)-1);
    rec.key[sizeof(rec.key)-1]='\0';
    rec.key_len=strlen(key);
    strncpy(rec.value,value,sizeof(rec.value)-1);
    rec.value[sizeof(rec.value)-1]='\0';
    rec.value_len=strlen(value);
    if (record_atomic_write(db->path,db->fd,rec_no,&rec,&db->hdr)==-1)
    {return -1;}
    index_insert(db->idx,key,rec_no);
    return 0;
}
int   fdb_delete(fdb_t *db, const char *key)
{
    int rec_no = index_find(db->idx,key);
    fdb_record_t rec;
    if (rec_no==-1)
    {return -1;}
    if (record_read(db->fd,rec_no,&rec)==-1)
    {return -1;}
    rec.flags=2;
    if (record_write(db->fd,rec_no,&rec)==-1)
    {return -1;}
    index_remove(db->idx,key);
    return 0;
}
int   fdb_list(fdb_t *db)
{
    fdb_record_t rec;
    for (int i=0;i<db->hdr.record_count;i++)
    {
        if (record_read(db->fd,i,&rec)==-1)
        {return -1;}
        if (rec.flags==1)
        {printf("%s\n",rec.key);}
    }
    return 0;
}
int   fdb_compact(fdb_t *db)
{
    fdb_record_t rec;
    char tmp_path[256];
    snprintf(tmp_path,sizeof(tmp_path),"%s.tmp",db->path);
    int fd = record_create(tmp_path,db->hdr.record_count);
    if (fd == -1)
    {return -1;}
    db->hdr.free_list_head=-1;
    for (int i=0;i<db->hdr.record_count;i++)
    {
        if (record_read(db->fd,i,&rec)==-1)
        {return -1;}
        if (rec.flags==1)
        {
            if (record_write(fd,i,&rec)==-1)
            {return -1;}
        }
        else
        {
            rec.flags=0;
            if (record_write(fd,i,&rec)==-1)
            {return -1;}
        }
    }
    if (fsync(fd)==-1)
    {return -1;}
    if (rename(tmp_path,db->path)==-1)
    {return -1;}
    close(db->fd);
    db->fd = fd;
    return 0;
}

