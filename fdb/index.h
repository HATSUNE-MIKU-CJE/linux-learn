#ifndef INDEX_H
#define INDEX_H

#include <stdint.h>

/* ========== 链式哈希表，key → record编号 ========== */
typedef struct index_entry {
    char key[64];
    int  rec_no;
    struct index_entry *next;
} index_entry_t;

typedef struct {
    int size;
    index_entry_t **buckets;
} index_t;

/* ========== 对外接口 ========== */
index_t* index_create(int size);
void     index_destroy(index_t *idx);

void     index_insert(index_t *idx, const char *key, int rec_no);
int      index_find(index_t *idx, const char *key);   /* 返回rec_no, -1=未找到 */
void     index_remove(index_t *idx, const char *key);

/* 启动时扫描全部record，建立索引 */
void     index_build(index_t *idx, int fd, uint32_t record_count);

#endif
