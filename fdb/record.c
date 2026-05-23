/* record.c — 磁盘读写层
 * 只关心"给定编号N，把字节写进去/读出来"，不懂key/value含义
 *
 * TODO: 实现以下函数
 *   record_read_header()   → lseek到0 → read header结构体
 *   record_write_header()  → lseek到0 → write header结构体
 *   record_read()          → lseek(HEADER_SIZE + N * RECORD_SIZE) → read
 *   record_write()         → lseek(HEADER_SIZE + N * RECORD_SIZE) → write
 *   record_atomic_write()  → 写tmp文件 → fsync → rename（保证崩溃一致性）
 *   record_create()        → 写header + 预分配record_count条空记录
 */
