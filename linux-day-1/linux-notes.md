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

