# logd — 日志守护进程 开发笔记

---

## 一、项目定位与体验

**功能一句话**：一个后台守护进程，开一条 FIFO 管道等着收日志，收到后加时间戳写进文件，文件太大了自动切。

**类型**：守护进程（Daemon）——启动后脱离终端，在后台持续运行，等待外部输入。跟前面三个 CLI 项目完全不同：CLI 是"启动→执行→退出"，守护进程是"启动→后台→永远等待→被信号叫停"。

**体感**：这是 Phase 1 里新概念密度最高的项目。守护进程化、FIFO、日志轮转、信号控制、`sig_atomic_t`、`strftime`……六七个新东西挤在一个 400 行的项目里。写起来断断续续，经常是刚拿起键盘发现要查 API，查完回来思路断了。但写完回头看，每个新概念其实就一层窗户纸——函数本身都只有几行代码。难的不是写，是不知道这些函数存在。

> 对比 bfile：bfile 难受是因为 C 语言底子不够（函数指针、大小端、字节对齐），logd 难受是因为 Linux 系统编程知识面不够。前者是深度问题，后者是广度问题。logd 这种"新概念密集"的项目，技术上更浅，但学习曲线更陡。

---

## 二、架构设计

logd 的功能很直接：收消息 → 格式化 → 写文件 → 超了切。不像 fdb 有复杂的磁盘格式和索引结构，logd 的数据流是一条直线。这让它的架构非常简单——两个文件，三个接口：

```
main.c — 进程生命周期
  ├── 解析 argv[1]=fifo路径, argv[2]=日志路径, argv[3]=大小上限
  ├── mkfifo 创建 FIFO 节点
  ├── 守护进程化（fork×2 + setsid + chdir + fd清理）
  ├── log_open 打开日志文件
  ├── 信号注册（SIGTERM→退出, SIGHUP→轮转）
  ├── open FIFO 读端 + 主循环（read → log_write）
  └── 退出清理（unlink FIFO + log_close）

log.c — 日志文件操作
  ├── log_open: 打开文件 + 分配结构体 + 取当前大小
  ├── log_write: 加时间戳 → 检查容量 → (超)轮转 → 写入
  ├── log_close: close + free
  └── 轮转逻辑: close → rename链 → open新文件
```

这种极简架构的优点：main.c 只管"数据从哪来"，log.c 只管"数据往哪去"。两者之间只传递一行文本——main.c 不需要知道文件怎么轮的，log.c 不需要知道数据是 FIFO 来的还是 socket 来的。

---

## 三、新知识点逐个拆

### 3.1 守护进程化（Daemon Initialization）

**一句话**：脱离终端，变成后台服务的标准六步。

```c
fork() != 0 → exit(0);           // 1. 父进程退出，子进程继续
setsid();                         // 2. 创建新会话，脱离控制终端
fork() != 0 → exit(0);           // 3. 再 fork，彻底失去"会话首领"身份
chdir("/");                       // 4. 切到根目录，防止占用挂载点
close(0); close(1); close(2);    // 5. 关掉继承来的 stdin/stdout/stderr
open("/dev/null", O_RDONLY);     // 6. 重定向到 /dev/null（防误写）
open("/dev/null", O_WRONLY);
open("/dev/null", O_WRONLY);
```

每步的理由：

| 步骤 | 不用会怎样 |
|------|-----------|
| fork + 父退 | 终端卡住，关了终端进程就死 |
| setsid | 终端关闭时 SIGHUP 会杀你 |
| 再 fork | 微弱的重新关联终端风险 |
| chdir("/") | 目录被占用，别人 umount 不了 |
| 关三 fd | 泄露文件描述符 |

这六步是标准模板，不用理解到内核层面，会背会抄就行。内核驱动开发里有时候你也要手写守护进程（用户态的辅助 daemon）。

> **引申：为什么 open("/dev/null") 自动拿到了 fd 0、1、2？**
> 内核分配 fd 的规则是"取当前最小的未使用编号"。你刚 close 了 0，open 返回的就是 0。同理 1 和 2。这就是 POSIX 的 fd 分配语义——不是魔法，是设计。

---

### 3.2 FIFO —— 命名管道

**一句话**：有名字的 pipe，任意两个进程通过文件路径找到对方。

pipe 只能在 fork 的父子进程之间通信（fd 通过 fork 继承）。FIFO 通过文件系统路径来对接：

```c
// 进程A（logd）
mkfifo("/tmp/logd.fifo", 0666);        // 创建文件节点
int fd = open("/tmp/logd.fifo", O_RDONLY);  // 打开读端

// 进程B（客户端，完全无关的进程）
int fd = open("/tmp/logd.fifo", O_WRONLY);  // 打开写端
write(fd, "hello", 5);
```

两个进程打开同一个路径，内核把它们的 fd 对接上。之后就跟 pipe 一样——字节流，单向，先进先出。

**FIFO vs pipe**：

| | pipe | FIFO |
|--|------|------|
| 怎么找到对方 | fork 继承 fd | 文件路径 |
| 适用场景 | 父子进程 | 任意进程 |
| 创建方式 | `pipe()` 一下拿两端 | `mkfifo` + 两次 `open` |
| 生命周期 | 随进程消失 | 文件节点一直在，直到 `unlink` |

**FIFO 的阻塞行为（坑）**：
- `open(fifo, O_RDONLY)` 会阻塞，直到有人 `open` 写端
- `open(fifo, O_WRONLY)` 会阻塞，直到有人 `open` 读端
- 如果所有写端都关了，读端 `read` 返回 0（EOF），需要关掉重新 `open` 等下一个写者

logd 的解决办法：读端在 `n==0` 时关掉重开，永远等着。

**FIFO 的精妙之处**：一个 FIFO 可以有多个写者同时打开。内核把多路写入串行化，读者一条条收。这让 logd 天然支持多客户端——比你为每个客户端开一个 pipe 简单无数倍。

---

### 3.3 信号作为外部控制通道

**一句话**：守护进程没有终端，信号就是你遥控它的唯一方式。

之前 mini-shell 用信号是"被动通知"——子进程死了，内核自动发 SIGCHLD，handler 收尸。logd 用信号是"主动控制"——管理员敲 `kill` 命令遥控守护进程：

```bash
kill <pid>        # 发 SIGTERM → logd 优雅退出
kill -HUP <pid>   # 发 SIGHUP  → logd 手动触发轮转
```

| | mini-shell (SIGCHLD) | logd (SIGTERM/HUP) |
|--|---------------------|-------------------|
| 谁发的 | 内核自动 | 管理员手动 |
| 目的 | 通知"子进程死了" | 控制"该干嘛" |
| 性质 | 被动监听 | 外部遥控 |

**handler 只改标记位，不做实际操作**：

```c
static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t need_rotate = 0;

void sig_handler(int sig) {
    if (sig == SIGTERM) running = 0;
    if (sig == SIGHUP)  need_rotate = 1;
}
```

信号随时到——可能正好在你 `write` 写到一半的时候打断你。如果在 handler 里操作 fd、调 `log_write`、`free`，可能搞乱文件状态。正确做法：handler 只改小标记，主循环看到标记后安全地处理。这和 STM32 的 ISR 设标记 + 主循环处理是一个道理。

---

### 3.4 sig_atomic_t

**一句话**：能被信号打断也不会读歪的整数类型。

普通 `int` 在 32 位系统上读写可能需要多条指令。信号在中间打断，handler 改了值，回来继续读——读到的是"半个旧值 + 半个新值" = 乱码。这叫做**撕裂读取（Torn Read）**。

`sig_atomic_t` 保证读写是一条指令完成，不会被信号撕裂。配合 `volatile`（告诉编译器每次都从内存读，别缓存到寄存器），就是信号处理的标准搭配。

```c
static volatile sig_atomic_t running = 1;   // 标准句式，可复用
```

实际在大多数平台上 `sig_atomic_t` 就是 `int`。但代码写 `sig_atomic_t` 而不是 `int`，是一种语义承诺——"这个变量是信号 handler 和主循环共享的"。类型即文档。

---

### 3.5 日志轮转（Log Rotation）

**一句话**：日志文件超过阈值就切走，换个新文件继续写。

```
app.log      ← 当前在写（500字节）
app.log.1    ← 上次的（379字节）
app.log.2    ← 更早的（800字节）
...
app.log.5    ← 最老的，再轮就会被覆盖
```

**轮转逻辑**：

```c
close(log->fd);                           // 1. 关掉当前文件

// 2. 从大到小，把 app.log.i 改名 app.log.(i+1)
for (int i = LOG_MAX_ROTATE - 1; i >= 1; i--) {
    rename("app.log.4", "app.log.5");     // 失败→源不存在，无视
    rename("app.log.3", "app.log.4");     // 同上
    ...
    rename("app.log.1", "app.log.2");     // 成功！
}

rename("app.log", "app.log.1");           // 3. 当前文件改名

log->fd = open("app.log", O_WRONLY | O_CREAT | O_APPEND, 0644);  // 4. 开新文件
log->current_size = 0;                    // 5. 重置大小
```

**关键设计决策：必须从大到小挪**。如果先 `rename(app.log → app.log.1)`，原来的 `app.log.1` 就被覆盖了，数据丢失。从 `.4 → .5` 开始，不存在就算了（rename 返回 -1 直接无视），保证已有数据不丢。最后才把当前文件变成 `.1`。

**为什么要 SIGHUP 触发手动轮转？** 自动轮转只在 `log_write` 检查——如果一小时没写日志，文件早超了也没触发。管理员发 `kill -HUP` 强制轮转。实现方法：把 `current_size` 临时设成 `max_size`，让下一次 `log_write` 自动检测到超限。

> **引申：inode 与 rename 的本质**
> Linux 上你可以 rename 一个正在被打开的文件——fd 指向的是 inode（内核对象），rename 改的是目录项（文件名到 inode 的映射）。所以 logd 对 `app.log` 的 fd 仍然有效，哪怕文件已经被 rename 成了 `app.log.1`。关掉再开 `app.log` 就是全新的文件（新的 inode）。这个机制是 fdb 原子写入的基础（tmp → rename → 旧文件 inode 被替换），也是 logd 轮转的基础。

---

### 3.6 strftime 时间戳格式化

**一句话**：把时间戳格式化成人类可读的字符串。

```c
#include <time.h>

time_t rawtime;
struct tm *info;
char buffer[80];

time(&rawtime);                              // 获取当前时间（从1970年1月1日0时UTC起经过的秒数）
info = localtime(&rawtime);                  // 转成本地时间（考虑时区，格式化为年月日等字段的结构体）
strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);  // 按格式输出字符串

snprintf(line, sizeof(line), "[%s] %s\n", buffer, msg);  // 拼接时间戳+消息
// 结果: [2026-05-30 22:30:01] I2C timeout
```

三个函数：
1. `time()` — 拿秒数（epoch）
2. `localtime()` — 秒数 → 年月日时分秒的结构体
3. `strftime()` — 结构体 → 格式化字符串

`%Y-%m-%d %H:%M:%S` 每个字母的含义很容易猜：Year、Month、Day、Hour、Minute、Second。

---

### 3.7 off_t —— 文件偏移量的专用类型

**一句话**：跟文件偏移/文件大小相关的变量，一律用 `off_t`。

```c
off_t size = lseek(fd, 0, SEEK_END);   // lseek 返回的就是 off_t
```

为什么不直接 `int` 或 `long`？
- `int` 在 32 位系统上最大 2GB——日志文件超过就溢出
- `size_t` 表示内存对象大小，语义不匹配文件偏移
- `off_t` 是内核为"文件偏移"这个语义专设的类型，读代码的人一眼就知道它存的是文件位置

在 64 位 Linux 上 `off_t = long = 8 字节`，足够大。类型是代码里的隐式文档。

---

### 3.8 O_APPEND —— 原子追加

```c
open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
```

`O_APPEND` 保证每次 `write` 都追到文件末尾——即使多个进程同时往同一文件写，也不会交叉覆盖。内核在 `write` 之前自动 `lseek` 到末尾，且这个"lseek + write"是原子的。

logd 是单进程，用 `lseek` + `write` 也安全，但 `O_APPEND` 额外保证万一将来有人改成多线程也不会炸。

---

## 四、数据流全景

```
客户端A ──write──┐
客户端B ──write──┤
                 ├──> [FIFO: /tmp/logd.fifo] ──read──> logd 守护进程
客户端C ──write──┘                                       │
                                                   加时间戳
                                                         │
                                                   检查文件大小
                                                         │
                                               ┌── 超了 ──> 轮转
                                               │
                                               └── 没超 ──> write
                                                         │
                                                   [日志文件]
```

配对的四类数据：

| 类别 | 内容 | 在哪 |
|------|------|------|
| 配置数据 | FIFO路径、日志路径、大小上限 | argv → `log_t` 结构体 |
| 传输数据 | 客户端发来的原始文本 | FIFO 字节流 |
| 格式化数据 | `[时间戳] 原始文本` | `line[4096]` 栈变量 |
| 持久化数据 | 日志文件 + `.1 ~ .5` | 磁盘 |

---

## 五、main.c 的主循环

```c
while (running) {
    ssize_t n = read(fifo_fd, buf, sizeof(buf) - 1);

    if (n > 0) {
        buf[n] = '\0';                       // 加结尾符
        char *nl = strchr(buf, '\n');        // 去换行
        if (nl) *nl = '\0';

        if (need_rotate) {
            log->current_size = log->max_size;  // 强制触发轮转
            need_rotate = 0;
        }

        log_write(log, buf);                 // 格式化 + 写文件（可能轮转）
    }
    else if (n == 0) {
        close(fifo_fd);                      // 写端全关 → 重开
        fifo_fd = open(fifo_path, O_RDONLY);
    }
}
```

这个循环不复杂——就是"读 → 处理标记 → 写文件"。比 bfile 的循环套分支清爽得多，比 fdb 的命令分发也简单。logd 的复杂度不在主循环，在新概念的学习成本上。

---

## 六、踩坑记录

**1. mkfifo 有个坑**：第二次启动 logd 时 FIFO 已经存在，`mkfifo` 返回 -1（errno = EEXIST）——这不是错误。`mkdir -p` 有 `-p` 忽略已存在，`mkfifo` 没有——要手动检查 `errno != EEXIST`。

**2. strncpy 被 snprintf 替代**：`log_open` 里复制路径，一开始用 `strncpy` + 手动补字符串结尾的 `\0`。后来发现 `snprintf(log->path, PATH_MAX, "%s", path)` 一行搞定，不需要额外补结尾符。两个函数都能用，但 `snprintf` 更简洁。

**3. 轮转循环必须从大到小**：如果先 `rename(app.log → app.log.1)`，会把已存在的 `app.log.1` 直接覆盖。从 `.4 → .5` 开始，从大到小逐级挪，保证不丢数据。第一次轮转时 `.1 ~ .4` 全不存在，rename 全部失败——这是正常的，直接无视。

**4. 信号 handler 绝不能写文件**：handler 里操作 fd = 未定义行为。只改 `sig_atomic_t` 标记位，主循环处理。

**5. 没有 log_t 结构体保护**：`fdb` 的 header 有 magic number（魔数校验），`bfile` 也有格式串做类型校验。logd 没有任何格式保护——客户端随意往 FIFO 写，logd 照单全收。这对于日志收集程序来说是合理的（日志本来就是自由文本），但也是一个明显的结构弱点。

---

## 七、项目分析方法的升级

logd 是第一个用**功能驱动法**分析的项目。旧的三步走（找数据→找操作→定模块）在概念密度高的时候跑不动——你连 FIFO 是什么都不知道，怎么"找数据"？

**功能驱动法**：

```
第一步：列功能（用户视角，自然语言）
第二步：功能 → 技术栈映射（标记"已会"和"待学"）
第三步：按技术边界定模块
```

logd 用了这个新方法后，分析流程变得顺畅——先确认程序要干嘛，再逐个功能查需要什么 syscall，最后自然落到 main.c（管进程生命周期与数据入口）和 log.c（管文件与轮转）两个模块。bfile 那种"先分析数据却卡在不知道概念"的问题消失了。

> 两种方法是互补的，不是替代的。数据结构驱动的项目（fdb）用旧三步走更快，概念密度高的项目（logd、bfile）用功能驱动法更顺。shdb 这种融合项目可以两种方法叠加使用。

---

## 八、总结

logd 400 行代码，新概念数量是 Phase 1 之最。但每个新概念拆开看都不复杂——守护进程是固定六步、FIFO 就是有名字的 pipe、信号是之前学过的换个用法、轮转是一串 rename。

这个项目的核心收获不是"学会了守护进程怎么写"——那六步你下次可能忘了，查一下就行。真正的收获是：**遇到一个由多个不熟悉概念拼成的项目时，怎么通过功能列表逐个击破。** 这种"拆解未知问题的能力"比记住具体 API 重要得多。

| 概念 | 难度 | 掌握 |
|------|:--:|:--:|
| 守护进程化 | ★★ | 六步模板，会背会用 |
| FIFO 命名管道 | ★★ | 已理解，和 pipe 对比清晰 |
| sig_atomic_t | ★ | 标准句式 |
| 信号控制（SIGTERM/SIGHUP） | ★★ | 遥控守护进程的模式 |
| 日志轮转（rename 链） | ★★ | 从大到小，不丢数据 |
| strftime 时间戳 | ★ | 已掌握 |
| off_t 文件偏移 | ★ | 类型即文档 |
| O_APPEND 原子追加 | ★ | 新标志位 |
| 功能驱动分析法 | ★★★ | 可迁移到任何新项目 |

**代码量**：~400 行（两个 .c + 一个 .h），三个核心函数，轮转逻辑全在 `log_write` 里。

> logd 项目完结。下一个项目：**shdb — 嵌入式时序数据库**（Phase 1 Boss 战，四个项目的深度融合）。
