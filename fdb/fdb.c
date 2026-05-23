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
