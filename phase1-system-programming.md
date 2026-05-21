# Linux C 学习笔记

> 从 STM32 裸机到 Linux 系统编程

---
## 1. 第一个 Linux C 程序

```c
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    printf("Hello, Linux! PID = %d\n", getpid());
    return 0;
}
```

编译运行：`gcc hello.c -o hello && ./hello`

### STM32 vs Linux 视角差异

| STM32 裸机 | Linux |
|------------|-------|
| main 是世界的中心 | main 只是几百个进程之一 |
| 你的代码是芯片上跑的全部 | 你前面 shell/内核/守护进程已在跑 |
| main 启动文件跳转 | 内核 fork+exec 创建进程 |
| return 后芯片停止 | return 0 返回给父进程（shell） |

---

## 2. PID（进程 ID）

**PID 是内核给进程分配的临时身份证号。**

- `getpid()` 返回当前进程的 PID
- 每次运行可能不同（内核动态分配）
- PID 不是你程序算出来的，是内核给的 → 这是和内核的第一次交互

---

## 3. printf 到底做了什么

**STM32：`printf` → 往 UART 数据寄存器里塞字节 → TX 引脚发出**

**Linux：`printf` → stdout 缓冲区 → `write()` 系统调用 → 内核 → 终端驱动 → 屏幕**

```
printf()           ← C 标准库格式化
    ↓
stdout 缓冲区       ← 攒够了一起发
    ↓
write() 系统调用    ← 唯一能跨进内核的通道
    ↓
内核 VFS 层        ← 判断 stdout 是终端设备
    ↓
终端驱动 (tty)     ← 内核终端子系统
    ↓
屏幕显示
```

**关键：用户程序没有权限碰硬件，只能通过系统调用让内核代劳。**

### strace — 抓系统调用

```bash
# 安装
sudo apt install strace -y

# 查看 write 系统调用
strace -e write ./hello
```

输出解读：
```
write(1, "Hello, Linux! PID = 2006\n", 25) = 25
      │                          │         │
      │                          │         └── 返回值：实际写了 25 字节
      │                          └── 要写的数据（24个字符 + 1个换行）
      └── 文件描述符 1 = stdout
```

**注意：strace 输出中没有 `printf`，因为内核只认识 `write`。**

---

## 4. 文件描述符（File Descriptor）

**文件描述符就是一个整数，指向一个"通道"。只管往里写、往外读，不关心通道那头是什么。**

### 和 STM32 的类比

| STM32 | Linux |
|-------|-------|
| USART_SendData(USART1, data) | write(1, data, n) |
| I2C_SendData(I2C1, data) | write(3, data, n) |
| SPI_SendData(SPI1, data) | write(5, data, n) |

接口完全相同，区别只在第一个参数是什么数字。

### 每个进程启动时内核自动打开的三个 fd

| fd | 名字 | 含义 | 方向 |
|----|------|------|------|
| 0 | stdin | 标准输入 | 键盘 → 程序 |
| 1 | stdout | 标准输出 | 程序 → 屏幕 |
| 2 | stderr | 标准错误 | 程序 → 屏幕（专用于报错） |

---

## 5. 重定向

**`1>/dev/null`：把 fd=1 的通道另一头从终端屏幕换成黑洞。**

```bash
./fd_test            # stdout 和 stderr 都到屏幕
./fd_test 1>/dev/null  # stdout 被吞掉，只剩 stderr
./fd_test 2>/dev/null  # stderr 被吞掉，只剩 stdout
```

- `/dev/null`：特殊设备文件，写进去的数据直接丢弃
- **重定向发生在程序启动之前**，shell（bash）在你程序跑起来之前就接好了通道
- 程序代码一行不改，行为却变了 → "一切皆文件"哲学的威力

---

## 6. fd 分配规则 & open()

**内核分配 fd 的规则：给你当前没被占用的、最小的那个整数。**

0/1/2 已被占用 → 第一次 `open()` 拿到 3，第二次拿到 4。

```c
int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

### open() 第二参数：模式 + 行为（按位或拼起来）

| 标志 | 含义 |
|------|------|
| O_RDONLY | 只读 |
| O_WRONLY | 只写 |
| O_RDWR | 读写 |
| O_CREAT | 文件不存在就创建 |
| O_TRUNC | 打开时清空文件内容 |
| O_APPEND | 追加模式，写到文件末尾 |

### open() 第三参数（八进制权限，仅 O_CREAT 时生效）

| 数字 | 权限 | 含义 |
|------|------|------|
| 0 | --- | 无权限 |
| 4 | r-- | 只读 |
| 6 | rw- | 读写 |
| 7 | rwx | 读写执行 |

`0644` = 用户可读写，同组只读，其他人只读。

---

## 7. read — 从文件描述符读数据

**一句话**：read 从内核缓冲区拉数据到用户态数组。fd 可以是普通文件、pipe、终端、socket——read 不关心 fd 指向什么。

```c
#include <unistd.h>
ssize_t read(int fd, void *buf, size_t count);
```

| 参数 | 含义 |
|------|------|
| fd | 从哪个通道读 |
| buf | 读到哪（用户态 buffer，你提供的） |
| count | 最多读多少字节（buffer 大小） |

**返回值**：

| 返回值 | 含义 | 什么时候发生 |
|--------|------|-------------|
| > 0 | 实际读到的字节数 | 有数据可读 |
| == 0 | EOF，文件读完了 | 普通文件到末尾；pipe 写端全关了 |
| -1 | 出错 | fd 无效、权限不够、被信号打断等 |

### 关键认知 1：返回值可能小于 count

**你请求读 100 字节，read 可能只返回 30。这不是 bug，是常态。**

原因：
- 普通文件：当前光标到文件末尾可能只有 30 字节了
- pipe：缓冲区里当前只有 30 字节（写端还没写完下一批）
- 终端：用户只输入了 30 字节就按了回车
- 网络 socket：TCP 分包到达，第一个包就 30 字节

**所以 read 不能这样写**：

```c
// 错误：假设一定读满
char buf[100];
read(fd, buf, 100);     // 可能只读了 30 字节！
printf("%s", buf);       // buf[30] 之后是垃圾
```

**正确的循环读**（读完指定字节数）：

```c
ssize_t read_full(int fd, void *buf, size_t total) {
    size_t done = 0;
    while (done < total) {
        ssize_t n = read(fd, (char*)buf + done, total - done);
        if (n == 0) break;          // EOF，提前结束
        if (n < 0) return -1;       // 出错
        done += n;
    }
    return done;
}
```

### 关键认知 2：阻塞行为取决于 fd 类型

| fd 类型 | 有数据时 | 无数据时 |
|---------|---------|---------|
| 普通文件 | 立即返回，返回可用字节 | 立即返回 0（EOF） |
| pipe 读端 | 立即返回 | **阻塞等待**（直到有数据或写端全关） |
| 终端 stdin | 立即返回 | **阻塞等用户输入** |
| socket | 立即返回 | **阻塞等数据到达** |

**普通文件不会阻塞，pipe/终端/socket 会阻塞。** 这就是为什么你写 pipe_demo 时子进程的 read 会停在那等父进程写数据——因为 pipe 读端没有数据时会阻塞。

### 关键认知 3：read 不保证读到的和 write 的边界一致

pipe 是字节流，没有消息边界。写端调了两次 `write(fd, "hello", 5)` 和 `write(fd, "world", 5)`，读端一次 `read(fd, buf, 10)` 可能一次就读到 `"helloworld"`，分不出原本是两个 write。**协议设计上要么用定长消息，要么自己加分隔符。**

---

## 8. write — 向文件描述符写数据

**一句话**：write 把用户态数组的数据推进内核缓冲区。和 read 对称。

```c
#include <unistd.h>
ssize_t write(int fd, const void *buf, size_t count);
```

| 参数 | 含义 |
|------|------|
| fd | 往哪个通道写 |
| buf | 要写的数据在哪 |
| count | 写多少字节 |

**返回值**：

| 返回值 | 含义 |
|--------|------|
| > 0 | 实际写入的字节数 |
| == 0 | 写了 0 字节（罕见，通常表示 fd 以 O_WRONLY 打开但设备不接受数据） |
| -1 | 出错 |

### 关键认知 1：write 也可能写不完

**你请求写 100 字节，write 可能只写 30。** 原因：pipe 缓冲区快满了、磁盘配额到了、被信号打断。

**正确的写全模式**：

```c
ssize_t write_all(int fd, const void *buf, size_t total) {
    size_t done = 0;
    while (done < total) {
        ssize_t n = write(fd, (const char*)buf + done, total - done);
        if (n < 0) return -1;
        done += n;
    }
    return done;
}
```

### 关键认知 2：write 和 `\0` 无关

write 只关心你告诉它的字节数。它不会找 `\0`，不会加 `\0`，文件/PIPE 里存的就是原始字节。这和 C 字符串函数（strlen/strcpy）完全不同。

```c
char data[] = {'H', 'i', '\0', '!'};
write(fd, data, 4);   // 写入 4 个字节，包括中间的 \0
// 文件里就是: 48 69 00 21
```

---

## 9. close — 释放文件描述符

```c
#include <unistd.h>
int close(int fd);
```

**close 做了两件事**：
1. 把 fd 号标记为空闲（后续 open/pipe 可能复用这个数字）
2. 递减内核里该"打开文件描述"的引用计数。减到 0 才真正释放

**pipe 的特殊意义**：close 写端是所有读端收到 EOF 的先决条件。不关写端，读端的 read 永远不会返回 0。

---

## 10. open — 打开文件/设备获取 fd

```c
#include <fcntl.h>
int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

### 第二参数：模式 + 行为（按位或拼起来）

| 标志 | 含义 |
|------|------|
| O_RDONLY | 只读 |
| O_WRONLY | 只写 |
| O_RDWR | 读写 |
| O_CREAT | 文件不存在就创建 |
| O_TRUNC | 打开时清空文件内容 |
| O_APPEND | 追加模式，写到文件末尾 |

### 第三参数（八进制权限，仅 O_CREAT 时生效）

| 数字 | 权限 | 含义 |
|------|------|------|
| 0 | --- | 无权限 |
| 4 | r-- | 只读 |
| 6 | rw- | 读写 |
| 7 | rwx | 读写执行 |

`0644` = 用户可读写，同组只读，其他人只读。

### fd 分配规则

**内核分配 fd 的规则：给你当前没被占用的、最小的那个整数。**

0/1/2 已被 stdin/stdout/stderr 占用 → 第一次 `open()` 拿到 3，第二次拿到 4。

---

## 11. 四个函数的关系总结

```
open(路径) → fd → read(fd, buf, n) / write(fd, buf, n) → close(fd)
pipe()     → fd → read/write 用法完全一样             → close(fd)
```

**写过文件 IO 就会用 pipe——唯一的区别是获取 fd 的方式不同**（open 给路径，pipe 直接返回），拿到 fd 之后的 read/write/close 完全一样。

这就是 Linux"一切皆文件"的体现：无论是磁盘文件、管道、终端、socket——对用户态来说都是 fd，都用一个 read 读、一个 write 写、一个 close 关。

---

## 12. 文件复制（read + write 串联）

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    char buf[64];
    int src_fd = open("test.txt", O_RDONLY);
    int dst_fd = open("test_copy.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    ssize_t n;
    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
        write(dst_fd, buf, n);
    }

    close(src_fd);
    close(dst_fd);
    return 0;
}
```

**read() 返回 0 = 文件读完。** 核心循环：
1. `read()` 读一块到缓冲区，返回实际字节数 n
2. n > 0：有数据，`write()` 原样写出
3. n == 0：文件读完，退出循环
4. n < 0：读取出错（这里先不处理）

---

## 13. lseek — 文件里的"光标"

**每个打开的文件都有一个隐藏的"当前位置"（文件偏移量），read/write 都从光标位置开始，操作完光标自动前移。**

类比 OLED 先 `SetPos(x, y)` 再写内容。

```c
#include <unistd.h>
off_t lseek(int fd, off_t offset, int whence);
```

| whence | 含义 | offset 含义 |
|--------|------|-------------|
| SEEK_SET | 从文件开头算 | 绝对位置 |
| SEEK_CUR | 从当前位置算 | 相对偏移（可为负数） |
| SEEK_END | 从文件末尾算 | 从末尾偏移 |

常用模式：
```c
lseek(fd, 0, SEEK_SET);   // 回到开头
lseek(fd, 0, SEEK_END);   // 移到末尾，返回文件大小
lseek(fd, 0, SEEK_CUR);   // 获取当前位置（不移动）
lseek(fd, 7, SEEK_SET);   // 跳到第7字节
```

`off_t` = 文件偏移量类型，64位系统上为 64 位有符号整数。

---

## 14. stat — 读取文件元数据（右键→属性）

**`stat()` 不打开文件，只读取文件的元数据（大小、权限、类型、时间戳），全部塞进 `struct stat`。**

类比 STM32 上读 EEPROM 的头部信息来确定数据块属性——信息是文件系统帮你维护的。

```c
#include <sys/stat.h>
#include <errno.h>

struct stat st;
if (stat("test.txt", &st) == -1) {
    perror("stat failed");
    return 1;
}
```

### struct stat 常用字段

| 字段 | 类型 | 含义 |
|------|------|------|
| st_size | off_t | 文件大小（字节） |
| st_mode | mode_t | 文件类型 + 权限（核心字段） |
| st_uid | uid_t | 属主用户 ID |
| st_nlink | nlink_t | 硬链接数 |
| st_ino | ino_t | inode 号 |
| st_atime | time_t | 最后访问时间 |
| st_mtime | time_t | 最后修改时间 |
| st_ctime | time_t | 最后状态变更时间 |

### st_mode 结构：类型 + 权限

`st_mode` 是一个 16 位整数，**前三位八进制 = 文件类型，后三位八进制 = 权限**：

```
100644  →  100 | 644
          类型 | 权限
```

| 掩码 | 作用 |
|------|------|
| `& 0777` | 只取权限位（后三位） |
| `& 0170000` | 只取类型位（前三位） |

### 文件类型宏（对 st_mode 做位运算）

| 类型位 | 宏 | 含义 |
|--------|-----|------|
| 100 | S_ISREG | 普通文件 |
| 040 | S_ISDIR | 目录 |
| 120 | S_ISLNK | 符号链接 |
| 020 | S_ISCHR | 字符设备（串口/键盘） |
| 060 | S_ISBLK | 块设备（硬盘/U盘） |
| 010 | S_ISFIFO | 命名管道 |

**关键认知：Linux 里文件类型由 inode 的 mode 位决定，和扩展名无关。**

### 三种变体

| 函数 | 区别 |
|------|------|
| stat(path, &st) | 跟随符号链接，取目标文件属性 |
| lstat(path, &st) | 不跟随符号链接，取链接自身属性 |
| fstat(fd, &st) | 通过已打开的 fd 取信息 |

### 硬链接（st_nlink）

**硬链接数 = 同一个 inode 被几个文件名指向。** 文件名和文件数据是分开存的——文件名是"标签"，inode 是"身份证"，数据块是"真身"。

```bash
ln test.txt test2.txt   # 创建硬链接，st_nlink 从 1 变成 2
```

`rm` 做的事：删掉目录项 + st_nlink 减 1。减到 0 时内核才真正释放 inode 和数据块（引用计数归零）。

目录的 st_nlink 至少为 2：目录名自身 + 内部的 `.`。

### 完整示例

```c
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

int main()
{
    struct stat st;
    if (stat("test.txt", &st) == -1) {
        perror("stat failed");
        return 1;
    }
    printf("Size:  %ld bytes\n", st.st_size);
    printf("Mode:  %o (octal)\n", st.st_mode & 0777);
    printf("UID:   %d\n", st.st_uid);
    printf("Links: %lu\n", st.st_nlink);

    if (S_ISREG(st.st_mode))
        printf("Type: regular file\n");
    else if (S_ISDIR(st.st_mode))
        printf("Type: directory\n");
    else if (S_ISCHR(st.st_mode))
        printf("Type: char device\n");
    return 0;
}
```

---

## 15. 目录操作 — opendir / readdir / closedir

**`opendir()` 打开目录拿到"目录流"句柄，`readdir()` 逐个读出目录里的文件名，`closedir()` 关掉。目录本质也是一个文件，里面存的是"文件名 → inode 号"的映射表。**

```c
#include <dirent.h>

DIR *d = opendir(".");
if (d == NULL) {
    perror("opendir");
    return 1;
}

struct dirent *e;
while ((e = readdir(d)) != NULL) {
    printf("%s\n", e->d_name);
}
closedir(d);
```

### struct dirent

```c
struct dirent {
    ino_t  d_ino;        // inode 号
    char   d_name[256];  // 文件名
};
```

### . 和 ..

每个目录都有内核自动生成的两个特殊条目：

| 名字 | 指向 | 含义 |
|------|------|------|
| `.` | 当前目录自身 | "我自己" |
| `..` | 父目录 | "上一层" |

过滤：`if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;`

### readdir 顺序

**不保证任何顺序。** 返回的是磁盘上条目存储的物理顺序，不同文件系统、目录增删后都可能不同。要排序自己 `qsort()`。

---

## 16. readdir + stat 组合 — 迷你 ls -l

**用 `snprintf` 拼出完整路径，再对每个文件调 `stat()`，拿到大小、权限、时间。**

### 权限转 rwx 格式 — 查表法

```c
const char *rwx[] = {"---","--x","-w-","-wx","r--","r-x","rw-","rwx"};

// 提取三位八进制位，直接当数组下标
printf("%s%s%s", rwx[(perm >> 6) & 7],
                 rwx[(perm >> 3) & 7],
                 rwx[(perm >> 0) & 7]);
```

查表法 = 数据驱动。比 switch-case 短，O(1) 无条件分支。

### 修改时间

```c
#include <time.h>
char *t = ctime(&st.st_mtime);  // "Sat May 18 21:30:00 2026\n"
t[24] = '\0';                    // 砍掉换行
```

### 完整示例

```c
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

int main()
{
    DIR *d = opendir(".");
    if (d == NULL) { perror("opendir"); return 1; }

    const char *rwx[] = {"---","--x","-w-","-wx","r--","r-x","rw-","rwx"};
    struct dirent *e;
    struct stat st;
    char path[1024];

    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "./%s", e->d_name);
        if (stat(path, &st) == -1) { perror("stat"); continue; }

        unsigned perm = st.st_mode & 0777;
        char *t = ctime(&st.st_mtime);
        t[24] = '\0';

        printf("%s%s%s %-20s %8ld bytes %s %.16s\n",
               rwx[(perm >> 6) & 7],
               rwx[(perm >> 3) & 7],
               rwx[perm & 7],
               e->d_name, st.st_size,
               S_ISDIR(st.st_mode) ? "DIR" : "FILE", t);
    }
    closedir(d);
    return 0;
}
```

### 注意

- `readdir()` 返回裸文件名，调 `stat()` 时必须拼完整路径
- `snprintf` 比 `sprintf` 安全，不会溢出
- GCC `-Wformat-truncation` 会在编译期推算缓冲区大小，不要忽略
- 路径缓冲区开 `[1024]` 省心智负担

---

## 17. fork — 进程分裂

**`fork()` 是 Linux 创建进程的唯一方式。调用一次，返回两次，把当前进程完整复制一份，父子进程从同一行代码继续往下跑。**

```c
#include <unistd.h>
pid_t pid = fork();
```

| 返回值 | 含义 |
|--------|------|
| > 0 | 在父进程，返回值 = 子进程 PID |
| == 0 | 在子进程 |
| < 0 | fork 失败 |

### 核心认知：if-else 不是判断了两次

fork 之后有两个执行者（父进程和子进程），**每个进程只看一个返回值，只走一条分支**。

```
fork() 返回 ─┬─ 父进程: pid = 子进程PID (6384) → 走 else if (pid > 0) 分支
             │
             └─ 子进程: pid = 0                → 走 if (pid == 0) 分支
```

fork 调用之前的代码只跑一次，fork 调用之后的所有代码在两个进程里各跑一遍。代码相同，执行流分叉。

### 调度顺序不确定

父子进程谁先被 CPU 执行，由内核调度器决定，**不保证顺序**。写代码时不能假设父先跑还是子先跑。

### shell 只等父进程

shell 只 wait 自己的直接子进程（父进程），不管"孙子"（子进程产生的子进程）。父进程退出时如果没有 wait 子进程：
- 子进程变**孤儿**，被 PID 1 (init) 收养
- shell 收回控制权，打印提示符
- 孤儿继续跑完，输出可能掉在 shell 提示符后面

这就是为什么会在终端看到 `$ [Child] my PID = xxx` 这种混乱输出。

### 最小示例

```c
#include <stdio.h>
#include <unistd.h>

int main()
{
    printf("Before fork, PID = %d\n", getpid());

    pid_t pid = fork();

    if (pid == 0)
        printf("[Child]  my PID = %d\n", getpid());
    else if (pid > 0)
        printf("[Parent] child PID = %d, my PID = %d\n", pid, getpid());
    else
        perror("fork failed");

    return 0;
}
```

### 与 FreeRTOS 对比

FreeRTOS 的 `xTaskCreate()` 创建的任务共享同一个地址空间，fork 创建的是独立地址空间的进程。fork 之后各进程有自己的内存拷贝（COW 优化）。

### 嵌入式中的用途

守护进程（daemon）的创建、shell 执行命令（fork + exec）、后台服务。以后写驱动时，传感器采集守护进程、日志写入进程都是 fork 出来的。

---

## 18. fork 炸弹与 COW 写时拷贝

### fork 炸弹 —— 子进程必须在循环里 break

```c
// 错误写法：指数爆炸
for (int i = 0; i < 5; i++) {
    pid = fork();   // 父子都继续循环，32 个进程
}

// 正确写法：子进程立刻跳出
for (int i = 0; i < 5; i++) {
    pid = fork();
    if (pid == 0) break;  // 子进程不准继续 fork
}
```

fork 后，到了第 i 轮时，前面创建的所有子进程也在跑循环体。不加 break → 2^n 爆炸。

### COW（Copy-On-Write，写时拷贝）

**fork 后父子共享同一块物理内存（标记只读），任意一方写时才触发内核拷贝一份。**

```
fork 刚执行完：  父 &num ──┐
                           ├── 物理页（共享，只读）
               子 &num ──┘

任意一方修改：   父 &num ──→ 物理页 A（原页）
               子 &num ──→ 物理页 B（新拷贝）
```

- 虚拟地址一样，物理页不同
- MMU 负责把相同的虚拟地址映射到不同的物理页
- 指针也跨不过这道墙——指针操作的是虚拟地址，MMU 按进程页表翻译

这是 MMU 的两个核心能力：**隔离 + 保护**。用户态程序崩了不会把内核搞挂。

---

## 19. 僵尸进程（Zombie）

**子进程死了，父进程不调用 wait() 收尸 → 子进程变僵尸，进程表里留 `<defunct>` 标记。**

- 僵尸已死，不占 CPU/内存，但占 PID 和进程描述符
- 用 `ps aux | grep Z` 查看：状态 `Z+`，带 `<defunct>`
- STM32 类比：中断 flag 置位后不清除，一直挂着

```c
// 制造僵尸：父进程 sleep 60s，子进程先死
if (pid > 0) {
    sleep(60);   // 期间子进程已死，没人收尸 → 僵尸
}
```

---

## 20. wait() — 父进程收尸

**`wait()` 让父进程阻塞等待，直到任意一个子进程退出，返回子进程 PID 并回收僵尸。**

```c
#include <sys/wait.h>

int status;
pid_t dead_child = wait(&status);  // 阻塞，直到有子进程退出
```

### 退出状态解析

| 宏 | 作用 |
|----|------|
| `WIFEXITED(status)` | 判断是否正常退出（return/exit） |
| `WEXITSTATUS(status)` | 提取退出码（return 值的低 8 位） |

```c
if (WIFEXITED(status)) {
    printf("Child exited with code %d\n", WEXITSTATUS(status));
}
```

### 核心认知

- 每次 wait() 回收一个僵尸
- 父进程死了（变孤儿被收养），僵尸才被 init/systemd 回收
- sleep 控制子进程死亡顺序 + wait 阻塞等待 = 实现父子同步

---

## 21. 孤儿进程（Orphan）

**爹比孩子先死 → 孩子变孤儿 → 被 init/systemd（PID=1）收养。**

```c
if (pid == 0) {
    // 子进程
    printf("Child PPID = %d\n", getppid());   // 第一次：亲爹
    sleep(5);
    printf("Child PPID = %d\n", getppid());   // 第二次：养父（PID 变了）
} else {
    // 父进程立刻死掉
    return 0;
}
```

- WSL2 上收养者不一定是 PID=1（可能是 900/1444），取决于 WSL init 架构
- 孤儿 ≠ 僵尸：孤儿是爹死了，僵尸是子死了爹不管

---

## 22. 竞态条件（Race Condition）

**fork 后父子并发执行，调度顺序不确定。不能假设谁先跑。**

### strace 实证

```bash
strace -f -tt ./orphan
```

微秒级时间戳证据：

```
19:01:03.849335  [子] write("Child ... PPID=13669")  ← 子先打
19:01:03.849378  [父] exit_group(0)                   ← 43 微秒后父才死
```

**子进程的第一次 printf 和父进程的 exit 只差 43 微秒**——调度器往哪边偏，结果就不同。

### 教训

多进程编程里不依赖"谁先谁后"的假设。必须显式同步——用 `wait()`、`sleep()`、信号量等手段保证顺序。

### 实验方法

```bash
# 跑 10 次看两种结果都有
for i in {1..10}; do ./orphan; echo "---"; done
```

相同代码，调度顺序不一样——就像 STM32 上两个中断同时触发，谁先响应看 NVIC 优先级仲裁。

---

## 23. 全文总结：STM32 裸机 → Linux 进程模型

| 概念 | STM32 裸机 | Linux |
|------|-----------|-------|
| 程序模型 | 一个 main 跑到底 | 多个进程并发，fork 分裂 |
| 内存 | 物理地址直接访问 | 虚拟地址 + MMU 隔离 |
| 创建新任务 | xTaskCreate 共享地址空间 | fork 独立地址空间 + COW |
| 父等子 | vTaskDelay / 信号量 | wait() 阻塞收尸 |
| 任务死亡 | 任务函数 return / 删除 | 子进程 return → 变僵尸 |
| 调度顺序 | FreeRTOS 优先级抢占 | CFS 调度器，不可依赖顺序 |
| 孤儿 | 不存在（所有任务同属一个程序） | 爹死子被 init 收养 |
| 调试 | 逻辑分析仪 / J-Link 单步 | strace 抓系统调用 |

---

## 24. exec 家族 — 进程变身

**`fork` 是复制自己，`exec` 是变成别人。fork+exec 是 Linux 创建新进程的万能公式。**

STM32 类比：bootloader 跳转到 APP —— 芯片还是同一颗，但跑的代码完全换了。exec 同样：PID/PPID/fd 表不变，代码/数据/栈被新程序替换。

### 家族一览

| 函数 | 程序来源 | 参数传递 | 环境变量 |
|------|---------|---------|---------|
| execl | 路径 | 逐个写 | 继承 |
| execlp | 文件名（PATH 搜索） | 逐个写 | 继承 |
| execvp | 文件名（PATH 搜索） | 数组 | 继承 |
| execve | 路径 | 数组 | 手动指定 |

**记法**：`l`=list（一个个写），`v`=vector（数组传），`p`=PATH 搜索，`e`=environment。

### 参数含义

```c
execlp("ls", "ls", "-l", NULL);
//      ①    ②    ③    ④
```

| 位置 | 值 | 含义 |
|------|-----|------|
| ① file | `"ls"` | 在 PATH 里搜索的可执行文件名 |
| ② argv[0] | `"ls"` | 传给新程序的程序名 |
| ③ argv[1] | `"-l"` | 传给新程序的第一个参数 |
| ④ 结尾 | `NULL` | 必须用 NULL 标记参数结束 |

**①和②含义不同**：①是文件系统路径，②是新程序眼里的 `argv[0]`。通常写一样即可。

### 核心认知

- **exec 成功绝不返回**：成功时当前进程的代码已被替换，`perror("exec failed")` 只在失败时才执行
- **fd 表保留**：exec 后之前打开的 fd 还在（这是 pipe+dup2+exec 的基础）
- **PID/PPID/父子关系不变**：进程是容器，程序是内容。exec 换内容不换容器

### execve — 底层系统调用

所有 exec 函数最终都调用 `execve`：

```c
extern char **environ;
char *args[] = {"/bin/ls", "-l", NULL};
execve("/bin/ls", args, environ);
```

---

## 25. waitpid — 非阻塞收尸

**`wait()` = 等任意子进程，死等。`waitpid()` = 可指定 PID、可非阻塞轮询。**

```c
pid_t waitpid(pid_t pid, int *status, int options);
```

| 参数 | 含义 |
|------|------|
| pid | 等哪个子进程（-1 = 等任意） |
| status | 退出状态 |
| options | `WNOHANG` = 非阻塞，0 = 阻塞 |

### WNOHANG 模式

```c
result = waitpid(-1, &status, WNOHANG);
// result > 0  → 收到子进程，返回其 PID
// result == 0 → 子进程都还活着，没收到
// result < 0  → 出错
```

STM32 类比：查中断标志位 —— `if (TIM1->SR & TIM_SR_UIF)`，有就处理，没有继续干活。

### 轮询+计数模式（守护进程标配）

```c
int reaped = 0;
while (reaped < 3) {
    result = waitpid(-1, &status, WNOHANG);
    if (result > 0) {
        printf("Child PID=%d done\n", result);
        reaped++;
    } else {
        // 没死，干自己的活
        sleep(1);
    }
}
```

---

## 26. Day4 总结

| 概念 | 核心 |
|------|------|
| exec 家族 | 进程变身另一个程序，PID 不变 |
| execl vs execv | l=参数逐个写，v=数组传 |
| execlp vs execve | p=PATH 搜索，不带 p 给完整路径 |
| execve | 底层系统调用，所有 exec 的终点 |
| fork+exec | shell 执行命令的万能公式 |
| waitpid | 可指定 PID、可非阻塞（WNOHANG） |
| 轮询+计数 | 守护进程管理子进程的标准模式 |

---

## 27. pipe — 匿名管道

**一句话**：pipe() 调用一次，内核返回两个 fd——一个读端、一个写端——它们共享内核里一块 FIFO 缓冲区。

```c
int fd[2];
pipe(fd);
// fd[0] = 读端, fd[1] = 写端
// 返回 0 成功, -1 失败
```

**pipe 就是内核里的 FIFO 队列**——一头入队，一头出队，字节先进先出。很像数据结构里的队列，一头输入另一头输出，同时先入先出。

### pipe 的三条硬规则

1. **单向**：fd[0] 只能读，fd[1] 只能写。双向需要两个 pipe
2. **字节流**：没有消息边界，两次 write 的内容可能被一次 read 全读走
3. **阻塞**：读空 pipe 会阻塞等数据；写满 pipe（默认 64KB 缓冲区）会阻塞等空间

### pipe 为什么必须配 fork

pipe 本身只在创建它的进程内有意义——自己写自己读没价值。fork 之后父子共享同一对 pipe fd（指向内核同一块缓冲区），一个进程写、另一个读，管道才有用。

**fork 之前**：只有父进程拥有 fd[0] 和 fd[1]
**fork 之后**：父子各有一套 fd[0] 和 fd[1]，但它们指向内核里同一个 pipe

### 关掉不用的端口——三条原因

1. **EOF 检测**：pipe 读端只有在**所有写端都关闭**时才会返回 EOF（read 返回 0）。如果父进程不关自己的读端，就算子进程写完了 close 了写端，子进程读的时候也永远不会收到 EOF——因为父进程手里还捏着一个写端没关
2. **fd 泄漏**：进程能打开的 fd 数量有限（默认 1024 个）
3. **防止自己误操作**：关了就不会手滑写错或读错

**规则**：谁写谁关读，谁关谁关写。父子都在 fork 之后立刻关掉自己不用的那一半。

### 阻塞行为

| 操作 | 行为 |
|------|------|
| read 有数据 | 立即返回，返回读到的字节数 |
| read 空 pipe，有写端还开着 | **阻塞等待**，直到有数据写入 |
| read 空 pipe，所有写端已关 | 返回 0（EOF，不阻塞） |
| write 没写满缓冲区 | 立即返回 |
| write 缓冲区满了 | **阻塞等待**，直到有空间 |

### 部分读问题

pipe read 是"有多少拿多少"——内核缓冲区里此刻有 200 字节就返回 200，不等到"所有数据到齐"。你不知道对方分几次写、每次写多少，所以必须循环读到 EOF：

```c
ssize_t n;
while ((n = read(fd[0], buf, sizeof(buf) - 1)) > 0) {
    buf[n] = '\0';
    printf("%s", buf);
}
```

**pipe read 不丢数据，只阻塞。** 如果写端还开着但数据暂时写完了，read 会死等（阻塞），不会"等不及直接结束"。循环不是防卡死，是防漏数据。

---

## 28. Miku 自述：pipe_demo 的四次 close

以下是 Miku 用自己的语言对 pipe_demo 中四次 close 的总结：

> 第一个 close，是子程序选择关闭写端，只保留读功能，然后读完了，再关闭读端，这是第二个 close。第三个 close，同理，是父程序选择关闭读端，只保留写端，再写完后，就关闭写端，这就是第四个 close。这些操作体现了 pipe 的单向导通，只能从一端输入，另一端输出，这倒是很像数据结构里的队列啊，一头输入另一头输出，同时又先入先出。

**补充**：父进程写完关 fd[1] 不只是"我写完了"——它在发信号："没有更多数据了，EOF。"子进程的 read() 之所以能从阻塞状态返回，就是因为它检测到了**所有写端都已关闭**，于是返回 0（EOF），子进程才继续往下跑。如果父进程写完不关 fd[1]，子进程的 read() 会**永远阻塞**在那。

> 读端收到 EOF 的充要条件 = 所有写端都被 close 了。

---

## 29. Miku 自述：两个通道

Miku 发现 pipe_demo 里跑了**两条独立的信息通道**：

> 两者之间原本是靠 pipe 通信的，子程序通过 pipe 知道父程序写完了（父程序关闭写通道），但父程序的结束，确实是由父子程序关系来控制的，也就是回收子程序的"僵尸"，得知子程序的死讯。两种通信方式，实现了这个程序。

| 通道 | 机制 | 传什么 | 方向 |
|------|------|--------|------|
| **数据通道** | pipe | 消息内容（"hello from parent"） | 父 → 子 |
| **生命周期通道** | fork 父子关系 + wait | "我死了"（退出状态） | 子 → 父 |

两个通道互不依赖。pipe 管数据传输，父子关系管进程回收。方向还是反的——数据从父流向子（pipe 写→读），死讯从子流向父（wait 回收僵尸）。后面写 Mini Shell 的时候两个通道会同时用——pipe 串命令输出输入，waitpid 收尸。

---

## 30. dup2 — fd 重定向

```c
int dup2(int oldfd, int newfd);
// 效果：newfd 变成 oldfd 的拷贝，指向同一个内核对象
```

dup2 不是"复制数据"，是**"让 newfd 号重新指向 oldfd 所指的那个内核对象"**。

```
执行前：
  1 ●──→ stdout (终端)
  4 ●──→ pipe 写端

dup2(4, 1);

执行后：
  1 ●──→ pipe 写端       ← stdout 被重定向到 pipe
  4 ●──→ pipe 写端       ← 4 没变
```

现在 printf 输出不再到屏幕，而是写进 pipe——因为 printf 内部调 write(1, ...)，而 fd=1 已经指向 pipe 了。

### dup2 后为什么还要 close 原始 fd

dup2 之后出现了两个指向 pipe 写端的 fd（fd=1 和原始 fd[1]），关掉原始 fd[1] 是为了：
1. 消灭冗余——反正已经有一个 fd（stdout）指向 pipe 了
2. 读端收到 EOF 的条件是**所有写端都关闭**——多一个写端口挂着，读端永远收不到 EOF

---

## 31. shell 管道的底层实现

`ls -l | grep foo` 在 shell 内部做了这些事：

```
1. pipe(fd)
2. fork 子进程1 (ls)
      dup2(fd[1], 1)      → ls 的 stdout → pipe 写端
      close(fd[0]); close(fd[1])
      exec("ls")
3. fork 子进程2 (grep)
      dup2(fd[0], 0)      → grep 的 stdin → pipe 读端
      close(fd[0]); close(fd[1])
      exec("grep foo")
4. 父进程 close(fd[0]); close(fd[1])
   wait 两个子进程
```

最终效果：`ls` 输出 → pipe → `grep` 输入。两个程序互不知道对方存在，只认 stdin/stdout。**shell/dup2 在程序不知情的情况下，把它们的输入输出线重新接了。**

---

## 32. Miku 自述：dup2_demo 完整流程分析

> 首先是创造了一个数组，用来储存 pipe 操作中间产生的 fd 信息，然后进行了 pipe 操作，此刻 fd 数组里就包含了 fd[0] 和 fd[1]，分别对应 pipe 读和 pipe 写。之后我们用 fork 创造了子程序。

> 子程序操作中，先关掉 pipe 读，然后使用重定向操作，将 1 定向到 fd[1]（也就是将"输出到终端"这个操作重新定向到输出到管道写，之后所有的"输出到终端"全部这样，变为输出到 pipe 写），但是此时此刻，pipe 写出现了两个写入口，一个原有，一个重定向的，那我们就关掉一个（关闭原有 fd[1]），然后再进行 execlp 操作，把 ls -l 拼凑起来发送到 pipe（本来是直接发给终端的，但重定向后，就给 pipe 了）。

> 最后是父程序，先关闭 pipe 写，然后创造储存数组，还有一个 n（用来确认什么时候读不到东西），在这个由 n 决定的循环中不断增加句末 '\0'，并不断输出。最后在读不到东西后，关闭 pipe 读，然后输出结束语。

**Miku 的思考**：说实话，这个地方真有点复杂吧。

**回应**：对，pipe + fork + dup2 + exec 四个东西拧在一起，是 Linux 系统编程里最绕的组合之一。能跑通说明逻辑已经对了。

---

## 33. Miku 自述：Linux 三层架构

Miku 对 Linux 岗位方向的理解：

> 内核开发，就是开发这种底层的东西，驱动开发，则是用开发好的内核，再次进行满足功能的开发，而应用开发，则是用开发好的驱动，去实现功能，一层套一层啊。

```
应用层（shdb / mini-shell）— 用户态
    │  write() / read() / ioctl()  ← 系统调用
    ▼
驱动层（/dev/sensor 设备节点）— 内核态
    │  file_operations { .read=..., .write=... }
    ▼
内核层（I2C子系统 / 进程调度 / 内存管理 / VFS）— 内核态
```

- **内核开发**：写进程调度器、内存管理、文件系统——改 Linux 源码树的核心代码
- **驱动开发**：写内核模块（.ko），插入内核框架，让新硬件可用——你的目标岗位
- **应用开发**：用系统调用写用户态程序，操作文件/设备/pipe——你现在写的代码

每一层只对上一层暴露接口，只调下一层提供的能力。驱动工程师坐中间——往上提供设备节点，往下调内核子系统和硬件寄存器。两层夹心，最吃框架理解和调试能力。

---

## 34. 信号 — 异步通知机制

**一句话**：信号是内核给进程发的"中断通知"——Ctrl+C 就是内核向前台进程发 SIGINT，进程默认反应是死。

### 信号 vs STM32 中断

Miku 自己抓到的对应关系：

> 在 STM32 和 FreeRTOS 里学中断的时候，就有一个 handler 函数对吧。

| | STM32 中断 | Linux 信号 |
|------|-----------|-----------|
| 事件源 | 硬件：GPIO 边沿、定时器溢出 | 内核：Ctrl+C、子进程死、kill |
| 注册 | NVIC_Init() 绑到中断向量表 | signal() / sigaction() 绑 handler |
| 响应 | CPU 打断当前指令，跳去跑 handler | 内核打断当前代码，跳去跑 handler |
| 恢复 | handler 跑完回原来地方继续 | 同 |
| 限制 | handler 里不能阻塞、长延时 | handler 里不能调非信号安全函数 |

本质是同一套机制：异步事件 → 打断当前执行流 → 跳到预设响应函数 → 处理完返回。区别：硬中断信号来自硬件引脚，Linux 信号来自内核。

### 核心函数

```c
#include <signal.h>

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
```

| 参数 | 含义 |
|------|------|
| signum | 要捕获的信号：SIGINT、SIGCHLD、SIGUSR1 等 |
| handler | 信号来时调的函数，或 SIG_IGN（忽略）、SIG_DFL（默认） |

handler 签名固定：`void handler(int sig)`，sig 是收到的信号编号。

### Miku 对 handler 的理解

> handler 就是"你先写好放着，等事情发生的时候有人替我调它"。说穿了就是："电话号码我给你了，有事打给我。"号码就是 handler 地址，信号就是那个电话。你从来不给自己打电话，但你随时准备接别人的电话。

### handler 的通信限制

handler 签名是内核规定的——`void handler(int sig)`，只能收一个信号编号，不能加参数。内核调 handler 时只传信号编号：

```c
handler(SIGINT);  // 内核只传这个，不传别的东西
```

**handler 和 main 之间只能通过全局/静态变量传信息。** 不是偷懒，是唯一的路。后面用 sigaction 也一样——这个限制来自信号机制的本质，改不了。

### 常用信号

| 信号名 | 触发条件 | 后续项目用在哪 |
|--------|---------|---------------|
| SIGINT | Ctrl+C | mini-shell 捕获它，只杀前台命令不杀 shell |
| SIGCHLD | 子进程死 | logd/shd 守护进程自动收尸 |
| SIGTERM | `kill <pid>` | 守护进程优雅退出（先 flush 再死） |
| SIGKILL | `kill -9 <pid>` | 强制杀，**不可被捕获/忽略** |
| SIGUSR1 | `kill -USR1 <pid>` | logd 收到后重新打开日志文件 |
| SIGUSR2 | `kill -USR2 <pid>` | logd 收到后切换调试级别 |
| SIGHUP | 终端断开 | 守护进程忽略它，防止终端关了跟着死 |

全部都是内核预定义的，只需 `signal(SIGxxx, handler)` 绑定即可。

---

## 35. SIGCHLD — 子进程死了自动通知

**轮询 vs 通知**：
- 之前：`waitpid(-1, &status, WNOHANG)` 轮询——父进程每隔一秒跑去看一圈"死了没？"
- 现在：子进程死的时候内核**主动通知**父进程，handler 里直接 `waitpid` 收尸，不浪费一秒

**关键：信号会合并。** 两个子进程同时死，SIGCHLD 可能只来一次。handler 里必须用 `while` 不是 `if`——一口气把所有僵尸收干净：

```c
void sigchld_handler(int sig) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // pid = 死掉的子进程 PID
        // 所有死掉的子进程一次性回收
    }
}
```

**与 pause() 配合**：父进程用 `while (alive > 0) pause()` 挂起，收到 SIGCHLD 唤醒一次，handler 里 `alive--`，全部收完退出循环。不浪费 CPU，不漏收尸。

---

## 36. kill — 主动发信号

```c
#include <signal.h>
int kill(pid_t pid, int sig);
```

别被名字骗了——kill 不止能杀人，它**发任意信号给任意进程**。

```c
kill(pid, SIGTERM);   // 礼貌地请进程退出
kill(pid, SIGKILL);   // 强制杀，不可被捕获/忽略
kill(pid, SIGUSR1);   // 用户自定义信号
kill(pid, 0);         // 不发送信号，只检查进程是否存在
```

**一个信号触发另一个信号**：在 SIGCHLD handler 里 `kill(getpid(), SIGUSR1)` 给自己发信号，两个 handler 链式调用——就像 STM32 上一个中断里软件触发另一个中断。

---

## 37. sigaction — 信号注册的标准方式

`signal()` 有两个毛病：(1) 不同 Unix 系统行为不一致，(2) 无法控制 handler 期间屏蔽哪些信号。`sigaction` 解决这两个问题。

```c
#include <signal.h>
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
```

### struct sigaction 主要字段

| 字段 | 含义 |
|------|------|
| sa_handler | handler 函数地址（和 signal 一样） |
| sa_flags | 行为标志，最常用 `SA_RESTART`（被信号打断的系统调用自动重试） |
| sa_mask | handler 执行期间要屏蔽的信号集 |

### 标准写法

```c
struct sigaction sa;
sa.sa_handler = my_handler;
sa.sa_flags = SA_RESTART;
sigemptyset(&sa.sa_mask);       // 清空屏蔽集——handler 期间允许其他信号来
sigaction(SIGINT, &sa, NULL);   // 注册
```

**头文件补充**：`sigaction`、`sigemptyset`、`SA_RESTART` 全部在 `<signal.h>` 里，无需额外 include。信号全部是内核预定义的宏，直接用名字即可。

### 常见编译问题

VSCode IntelliSense 可能误报 `struct sigaction` 不完整——这是 glibc 头文件嵌套太深导致 IDE 解析失败。只要 `gcc` 编译通过就没问题。若看着烦，在所有 include 之前加 `#define _POSIX_C_SOURCE 199309L` 强制 POSIX 标准模式。

---

## 38. 信号实验全记录

### 实验一：SIGINT 捕获 Ctrl+C

`sigchld_demo.c`（最初版本）：主循环空跑，按 Ctrl+C 不死，handler 计数器到 3 才退出。

### 实验二：SIGCHLD 自动收尸

fork 3 子进程，不同 sleep 时间后 return 不同退出码。父进程 `signal(SIGCHLD, ...)` + `while(alive > 0) pause()` 挂起等待。handler 里 `while+waitpid` 一口气收完所有僵尸。

### 实验三：kill 信号链

SIGCHLD handler 里 `kill(getpid(), SIGUSR1)` 给自己发信号，SIGUSR1 绑另一个 handler 打日志——一个信号触发另一个信号，两个 handler 链式调用。

### 实验四：signal → sigaction 替换

将两处 `signal()` 替换为 `struct sigaction sa/sb + sigemptyset + sigaction()`，功能完全等价，但 sigaction 是标准写法。
