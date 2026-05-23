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
