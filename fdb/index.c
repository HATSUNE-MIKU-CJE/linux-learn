/* index.c — 内存索引层（链式哈希表）
 * 只关心"key→编号"的映射，不懂文件格式
 *
 * TODO: 实现以下函数
 *   index_create()   → malloc index_t + bucket数组
 *   index_destroy()  → 释放所有entry + bucket + index
 *   index_insert()   → 哈希 → 链到对应bucket → 如key已存在则更新rec_no
 *   index_find()     → 哈希 → 遍历链表 → 返回rec_no（-1未找到）
 *   index_remove()   → 哈希 → 遍历链表 → 摘除entry → free
 *   index_build()    → 遍历所有record → 遇flags=VALID则index_insert
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "index.h"
#include "record.h"

// djb2 哈希算法
static unsigned int hash(const char *key, int size)
{
    unsigned int h = 5381;
    while (*key)
        h = ((h << 5) + h) + *key++;   // h * 33 + c
    return h % size;
}

index_t* index_create(int size)
{
    index_t *idx = (index_t*)malloc(sizeof(index_t));
    idx->size = size;
    idx->buckets = (index_entry_t**)malloc(size*sizeof(index_entry_t*));
    memset(idx->buckets,0,size * sizeof(index_entry_t*));
    return idx;
}

void index_destroy(index_t *idx)
{
    index_entry_t *p;
    index_entry_t *next;
    for (int i=0;i<idx->size;i++)
    {
        p = idx->buckets[i];
        while (p != NULL)
        {
            next = p->next;
            free(p);
            p = next;
        }

    }        
    free(idx->buckets);
    free(idx);
}
void index_insert(index_t *idx, const char *key, int rec_no)
{
    unsigned int num = hash(key,idx->size);
    index_entry_t *p;
    p = idx->buckets[num];
    while (p != NULL)
    {
        if (strcmp(key,p->key)==0)
        {
            p->rec_no = rec_no;
            return;
        }
        p = p->next;
    }
    p = idx->buckets[num];
    index_entry_t *entry = (index_entry_t*)malloc(sizeof(index_entry_t));
    entry->next = p;
    idx->buckets[num] = entry;
    strncpy(entry->key,key,sizeof(entry->key)-1);
    entry->key[sizeof(entry->key)-1] = '\0';
    entry->rec_no=rec_no;
}

int index_find(index_t *idx, const char *key)
{
    unsigned int num = hash(key,idx->size);
    index_entry_t *p;
    p = idx->buckets[num];
    while (p != NULL)
    {
        if (strcmp(key,p->key)==0)
        {
            return p->rec_no;
        }
        p = p->next;
    }
    return -1;
}

void index_remove(index_t *idx, const char *key)
{
    unsigned int num = hash(key,idx->size);
    index_entry_t *p = idx->buckets[num];
    index_entry_t *prev = NULL;
    while (p!=NULL)
    {
        if (strcmp(key,p->key)==0)
        {
            if (prev == NULL)
            {
                idx->buckets[num] = p->next;
            }
            else
            {
                prev->next = p->next;
            }
            free(p);
            return;
        }
        prev = p;
        p = p->next;
    }
}

void index_build(index_t *idx, int fd, uint32_t record_count)
{
    fdb_record_t rec;
    for (int i=0;i<record_count;i++)
    {
        if (record_read(fd,i,&rec)==-1)
        {
            break;
        }
        if (rec.flags == 1)
        {
            index_insert(idx,rec.key,i);
        }
    }
}
