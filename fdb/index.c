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
