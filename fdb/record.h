#ifndef RECORD_H
#define RECORD_H

#include "fdb.h"

/* ========== Header 操作 ========== */
int record_read_header(int fd, fdb_header_t *hdr);
int record_write_header(int fd, const fdb_header_t *hdr);

/* ========== Record 操作（按编号读写） ========== */
int record_read(int fd, int rec_no, fdb_record_t *rec);
int record_write(int fd, int rec_no, const fdb_record_t *rec);

/* ========== 原子写入 ========== */
int record_atomic_write(const char *path, int fd,
                        int rec_no, const fdb_record_t *rec,
                        const fdb_header_t *hdr);

/* ========== 创建新数据文件 ========== */
int record_create(const char *path, uint32_t record_count);

#endif
