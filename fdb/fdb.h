#ifndef FDB_H
#define FDB_H

#include <stdint.h>
#include <stddef.h>

/* ========== 常量 ========== */
#define FDB_MAGIC       0x46444230   /* "FDB0" */
#define FDB_VERSION     1
#define FDB_RECORD_SIZE 256
#define FDB_MAX_KEY_LEN 62
#define FDB_MAX_VAL_LEN 190
#define FDB_HEADER_SIZE 20

/* ========== 磁盘结构（packed 防止对齐填充） ========== */
typedef struct {
    uint8_t  flags;       /* 0=空闲, 1=有效, 2=tombstone */
    uint8_t  key_len;
    char     key[62];
    uint16_t value_len;
    char     value[190];
} __attribute__((packed)) fdb_record_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t record_size;
    uint32_t record_count;
    int32_t  free_list_head;  /* -1 表示无空闲槽位 */
} fdb_header_t;

/* ========== 对外 API ========== */
int   fdb_open(const char *path);
void  fdb_close(int db);

int   fdb_get(int db, const char *key, char *value, size_t len);
int   fdb_set(int db, const char *key, const char *value);
int   fdb_delete(int db, const char *key);
int   fdb_list(int db);
int   fdb_compact(int db);

#endif
