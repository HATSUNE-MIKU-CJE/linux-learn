# Linux C 学习笔记

> 从 STM32 裸机到 Linux 系统编程

---

## 6. fd 分配规则 & open() 系统调用

**内核分配 fd 的规则：给你当前没被占用的、最小的那个整数。**

0/1/2 已被占用 → 第一次 `open()` 拿到 3，第二次拿到 4，以此类推。

```c
#include <fcntl.h>
#include <unistd.h>

int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

### open() 常用标志

| 标志 | 含义 |
|------|------|
| O_RDONLY | 只读 |
| O_WRONLY | 只写 |
| O_RDWR | 读写 |
| O_CREAT | 文件不存在就创建 |
| O_TRUNC | 打开时清空文件内容 |
| O_APPEND | 追加模式，写到文件末尾 |

第三个参数 `0644`：八进制权限，6=用户可读写，4=同组可读，4=其他人可读。

### close() — 用完要关

```c
close(fd);
```

类比 STM32 上用完外设关时钟。在 Linux 里不关的话进程结束时会自动回收，但养成手动关的习惯。

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

## 7. open / read / write / close 总结

**核心只有四步，和 STM32 初始化外设 → 操作 → 收尾一样：**

| 步骤 | 写操作 | 读操作 |
|------|--------|--------|
| 1. 开通道 | `fd = open("test.txt", O_WRONLY)` | `fd = open("test.txt", O_RDONLY)` |
| 2. 操作 | `write(fd, msg, n)` — 推数据出去 | `n = read(fd, buf, size)` — 拉数据进来 |
| 3. 关通道 | `close(fd)` | `close(fd)` |

**`\0` 是 C 字符串的事，和文件无关。** 文件里存的就是原始字节，`read()` 读到什么就是什么。用 `printf("%s")` 打印时才需要手动补 `\0`，用 `write()` 则不需要。

---

## 8. 文件复制（read + write 串联）

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

## 9. lseek — 文件里的"光标"

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

## 10. stat — 读取文件元数据（右键→属性）

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
