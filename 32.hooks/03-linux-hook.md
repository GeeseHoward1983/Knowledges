# 03 · Linux 用户态与内核态 Hook

> 本篇覆盖 Linux 上从 **LD_PRELOAD** 到 **eBPF + libbpf + CO-RE** 的全套 hook 技术：ELF 动态链接 / GOT-PLT 机制、`dlsym(RTLD_NEXT, ...)`、ptrace 注入、kprobe / uprobe / tracepoint、bpftrace 入门、BCC、libbpf + CO-RE + BTF 深入示例。
> 假设读者：会 C，熟悉 Linux 系统编程、ELF 与动态库基础、有 `gdb`/`strace` 经验。

---

## 1. 概念地图

Linux 上 hook 技术按"挂载点位置"自上而下：

| 层 | 技术 | 是否需要 root | 是否需要重启进程 | 适用场景 |
|---|---|---|---|---|
| **应用层** | 直接替换函数 / 回调机制 | 否 | 否 | 自家代码 |
| **C 运行时层** | `LD_PRELOAD` + `dlsym(RTLD_NEXT)` | 否（除非目标是 setuid） | **是** | 替换 libc 函数（malloc、open、socket）|
| **进程注入** | `ptrace` / `gdb` | 否（同 uid） | 否 | 给已运行进程注入代码 |
| **内核 trace** | tracepoint / kprobe / uprobe + ftrace | 是 | 否 | 系统级追踪 |
| **eBPF** | BPF 程序 + verifier + JIT | 是（或 CAP_BPF）| 否 | 现代生产可观测性、安全 |
| **内核模块** | 自写 LKM | 是 | 否 | 极端定制（不推荐除非别无选择）|

**选择原则**：
- 你能改源码 → 用源码。
- 你能控制启动 → `LD_PRELOAD`。
- 你不能改启动但能 attach → `ptrace` / GDB。
- 你要看系统级（多进程） → eBPF。
- eBPF 解决不了 → 才考虑内核模块。

---

## 2. ELF 动态链接与 GOT/PLT —— LD_PRELOAD 的基础

要理解 `LD_PRELOAD`，必须先理解 ELF 动态库是怎么解析的。

### 2.1 GOT / PLT 简介

考虑一个 C 程序：

```c
[[include]] <stdio.h>
int main(void) { printf("hello\n"); return 0; }
```

`printf` 在 `libc.so.6` 里，但程序编译时不知道运行时 `libc.so.6` 加载到哪个地址。解决方案：**lazy binding via PLT/GOT**。

- **GOT（Global Offset Table）**：每个外部符号在程序数据段里有一个 8 字节槽，存运行时解析出的真实地址。
- **PLT（Procedure Linkage Table）**：每个外部符号有一段小桩代码（约 16 字节）。`call printf@plt` → 跳到 PLT 桩 → 第一次会调 ld.so 的 resolver 填好 GOT → 之后 PLT 桩直接 `jmp *GOT[i]`。

反汇编看：

```
$ objdump -d a.out | grep -A 3 'printf@plt'
0000000000401030 <printf@plt>:
  401030: ff 25 e2 2f 00 00     jmp    *0x2fe2(%rip)        # 404018 <printf@got>
  401036: 68 00 00 00 00        push   $0x0
  40103b: e9 e0 ff ff ff        jmp    401020 <.plt>
```

`%rip + 0x2fe2 = 0x404018` 就是 `printf` 的 GOT 槽。

### 2.2 LD_PRELOAD 的工作流程

`LD_PRELOAD=/path/to/my.so /path/to/program` 让 ld.so 在解析符号时**先在 my.so 里找**——如果 my.so 里有同名符号，所有调用都解析到 my.so 而非 libc.so.6。

```
ld.so 的符号搜索顺序:
1. LD_PRELOAD 指定的库
2. 可执行文件自身
3. DT_NEEDED 列出的库（依赖库，如 libc.so.6）
4. ld.so 默认搜索路径
```

这种"插入"对**全程序**生效——任何调用 `printf` 的代码都会进 my.so。

### 2.3 拿原函数：dlsym(RTLD_NEXT, ...)

如果 my.so 想"先打日志再调真 printf"，需要拿到原 `printf`。`dlsym(RTLD_NEXT, "printf")` 会让 ld.so 跳过自己（my.so）继续往下找——找到 libc.so.6 里的 printf。

### 2.4 完整示例：LD_PRELOAD 替换 malloc

```c
// preload_malloc.c
[[define]] _GNU_SOURCE
[[include]] <dlfcn.h>
[[include]] <stdio.h>
[[include]] <stdlib.h>
[[include]] <string.h>

typedef void *(*malloc_t)(size_t);
typedef void  (*free_t)(void*);

static malloc_t real_malloc = NULL;
static free_t   real_free   = NULL;

static __thread int in_hook = 0;   // 重入保护

static void resolve(void) {
    real_malloc = (malloc_t)dlsym(RTLD_NEXT, "malloc");
    real_free   = (free_t)  dlsym(RTLD_NEXT, "free");
}

void *malloc(size_t n) {
    if (!real_malloc) resolve();
    void *p = real_malloc(n);
    if (!in_hook) {                                 // 防止 fprintf 内部又 malloc 引发死循环
        in_hook = 1;
        fprintf(stderr, "malloc(%zu) = %p\n", n, p);
        in_hook = 0;
    }
    return p;
}

void free(void *p) {
    if (!real_free) resolve();
    if (!in_hook) {
        in_hook = 1;
        fprintf(stderr, "free(%p)\n", p);
        in_hook = 0;
    }
    real_free(p);
}
```

编译运行：

```bash
gcc -fPIC -shared preload_malloc.c -o libpremalloc.so -ldl
LD_PRELOAD=./libpremalloc.so ls /
```

输出会同时打印 `ls` 的目录内容和大量 `malloc(...) = ...` / `free(...)` 行。

**关键细节**：

1. `__thread int in_hook` 是**重入保护**——`fprintf` 内部还会调 `malloc`/`free`，没有保护会无限递归。
2. `dlsym` 自身也可能调 `malloc`——`real_malloc==NULL` 时不能 `fprintf`，否则崩。最稳的做法是 `__attribute__((constructor))` 提前解析。
3. **setuid 程序不会加载 `LD_PRELOAD`**——属于安全设计，避免本地提权。
4. **多线程下 `dlsym` 不可重入**——用 `pthread_once` 包一层更稳。

### 2.5 完整示例：替换 socket connect 做流量审计

```c
// preload_connect.c
[[define]] _GNU_SOURCE
[[include]] <dlfcn.h>
[[include]] <sys/socket.h>
[[include]] <netinet/in.h>
[[include]] <arpa/inet.h>
[[include]] <stdio.h>
[[include]] <pthread.h>

typedef int (*connect_t)(int, const struct sockaddr*, socklen_t);
static connect_t real_connect;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static void init(void) { real_connect = (connect_t)dlsym(RTLD_NEXT, "connect"); }

int connect(int sockfd, const struct sockaddr *addr, socklen_t len) {
    pthread_once(&once, init);
    if (addr && addr->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in*)addr;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip));
        fprintf(stderr, "[audit] connect %s:%u\n", ip, ntohs(in->sin_port));
    }
    return real_connect(sockfd, addr, len);
}
```

```bash
gcc -fPIC -shared preload_connect.c -o libaudit.so -ldl
LD_PRELOAD=./libaudit.so curl https://example.com
# stderr: [audit] connect 93.184.216.34:443
```

---

## 3. ptrace —— 给已运行进程注入

### 3.1 ptrace 简介

`ptrace(2)` 是调试器的基础：

| 请求 | 作用 |
|---|---|
| `PTRACE_ATTACH` / `PTRACE_SEIZE` | 附加到一个进程 |
| `PTRACE_PEEKDATA` / `PTRACE_POKEDATA` | 读写目标内存 |
| `PTRACE_GETREGS` / `PTRACE_SETREGS` | 读写寄存器 |
| `PTRACE_CONT` / `PTRACE_SYSCALL` / `PTRACE_SINGLESTEP` | 继续执行 |
| `PTRACE_DETACH` | 解除附加 |

要给目标进程注入 hook，思路：

1. `ptrace(PTRACE_ATTACH, pid)` 附加。
2. 保存目标当前寄存器。
3. 用 `PTRACE_POKEDATA` 在某可执行页写入一段"调用 `dlopen("/path/to/my.so", RTLD_NOW)`"的 shellcode（或改 RIP 指向已有的 `__libc_dlopen_mode`/`dlopen`）。
4. 改 RIP 指向 shellcode，`PTRACE_CONT`。
5. 等到 SIGTRAP（shellcode 末尾放 `int 3`），表示注入完成。
6. 恢复原寄存器和内存，detach。

实战工具：[**linux-inject**](https://github.com/gaffe23/linux-inject)、[**injector**](https://github.com/kubo/injector) 都封装了上述步骤。

### 3.2 简化示例：ptrace 让目标进程调 puts

```c
// inject_puts.c —— 极简，无错误处理；x86_64; 假设目标已加载 libc
// 用法: sudo ./inject_puts <pid>
[[include]] <sys/ptrace.h>
[[include]] <sys/user.h>
[[include]] <sys/wait.h>
[[include]] <sys/types.h>
[[include]] <stdio.h>
[[include]] <stdlib.h>
[[include]] <string.h>
[[include]] <unistd.h>

long find_libc_func(pid_t pid, const char *name);   // 自行实现：解析 /proc/<pid>/maps + 本进程 dlsym 的偏移

int main(int argc, char **argv) {
    pid_t pid = atoi(argv[1]);
    ptrace(PTRACE_ATTACH, pid, 0, 0);
    waitpid(pid, NULL, 0);

    struct user_regs_struct regs, saved;
    ptrace(PTRACE_GETREGS, pid, 0, &regs);
    saved = regs;

    long puts_addr = find_libc_func(pid, "puts");

    // 在栈底找一段空间写字符串
    const char msg[] = "hello from injector\n";
    long sp = regs.rsp - 128;
    for (size_t i = 0; i < sizeof(msg); i += 8) {
        long word = 0;
        memcpy(&word, msg + i, sizeof(msg) - i < 8 ? sizeof(msg) - i : 8);
        ptrace(PTRACE_POKEDATA, pid, sp + i, word);
    }

    // 设置参数 rdi = sp, rip = puts; 调用结束后让它 int 3 停下
    regs.rdi = sp;
    regs.rip = puts_addr;
    // 在 puts 返回时栈顶应该是返回地址 —— 我们放一个会触发 SIGSEGV 的地址让 ptrace 停下
    long bad_ret = 0;
    ptrace(PTRACE_POKEDATA, pid, regs.rsp - 8, bad_ret);
    regs.rsp -= 8;
    ptrace(PTRACE_SETREGS, pid, 0, &regs);

    ptrace(PTRACE_CONT, pid, 0, 0);
    int status; waitpid(pid, &status, 0);

    // 恢复
    ptrace(PTRACE_SETREGS, pid, 0, &saved);
    ptrace(PTRACE_DETACH, pid, 0, 0);
    return 0;
}
```

完整可用版本要做的额外事：
- 解析 `/proc/<pid>/maps` 找 libc 基址。
- 解析本地 libc 的 `puts` 偏移加到目标基址（同版本 libc）。
- 用 `mmap` syscall 在目标进程申请新内存页，避免在栈上写代码（NX 页问题）。

工程化建议：**直接用 gdb**：

```bash
gdb -p <pid>
(gdb) call (void)dlopen("/abs/path/libhook.so", 2)
(gdb) detach
```

GDB 内部走的就是 ptrace，但帮你做了 ABI 处理。

### 3.3 限制

- `/proc/sys/kernel/yama/ptrace_scope` 控制谁能 ptrace 谁。Ubuntu 默认 `1`（只允许 ptrace 自己的子进程），改 `0` 可任意 ptrace 同 uid 进程。
- 同进程同时只能被一个 tracer 附加（你 attach 就会把 gdb 踢掉）。

---

## 4. ftrace / kprobe / uprobe —— 内核追踪基础设施

### 4.1 ftrace

Linux 内核内置的函数追踪器。早年通过 `/sys/kernel/debug/tracing/` 配置：

```bash
echo function > /sys/kernel/debug/tracing/current_tracer
echo do_sys_open > /sys/kernel/debug/tracing/set_ftrace_filter
echo 1 > /sys/kernel/debug/tracing/tracing_on
cat /sys/kernel/debug/tracing/trace_pipe
# 看到所有 do_sys_open 的调用
echo 0 > /sys/kernel/debug/tracing/tracing_on
```

现代等价物是 `trace-cmd` 和 `perf trace`。`ftrace` 的底层是**函数序言里的 nop 被 patch 成 call**——内核编译时所有可追踪函数前都留了 5 字节 nop。

### 4.2 kprobe / uprobe

更通用的"在任意指令地址注入"机制：

- **kprobe**：内核函数任意指令处插桩，用 `int 3` 触发。
- **uprobe**：用户态函数任意指令处插桩，通过 `/sys/kernel/debug/tracing/uprobe_events` 注册。
- **kretprobe / uretprobe**：函数返回时触发。

经典命令（已基本被 eBPF 取代，了解即可）：

```bash
# 在 readline 函数返回时打印
echo 'r:bashret /bin/bash:0x1234 cmd=+0(%ax):string' > /sys/kernel/debug/tracing/uprobe_events
echo 1 > /sys/kernel/debug/tracing/events/uprobes/bashret/enable
cat /sys/kernel/debug/tracing/trace_pipe
```

---

## 5. eBPF —— 现代 Linux Hook 的统一答案

### 5.1 eBPF 是什么

**eBPF（extended Berkeley Packet Filter）**：在内核里运行**经过验证的、受限的字节码**程序，附着到各种 hook 点（kprobe、tracepoint、网络、cgroup、LSM、XDP……）。是过去十年 Linux 内核最重要的特性之一。

工作流：

```
用户态: 写 C → clang -target bpf 编译为 BPF 字节码
        ↓
内核 bpf() syscall: 上传字节码
        ↓
内核 verifier: 检查无非法内存访问、无无限循环、栈深度
        ↓
JIT: 字节码 → 本地机器码
        ↓
attach 到 hook 点 (kprobe / tracepoint / XDP / cgroup / ...)
        ↓
hook 触发 → BPF 程序运行 → 写入 map / ringbuf / perf event → 用户态读取
```

**优势**：
- 安全：verifier 保证 BPF 程序不会让内核崩溃。
- 性能：JIT 后接近原生。
- 可观测性：BPF 程序可读取内核任意数据结构。
- 动态：无需改源码、无需重启、无需加载内核模块。

**hook 点类型（一部分）**：

| 类型 | 触发时机 |
|---|---|
| `kprobe` / `kretprobe` | 内核函数 进入/返回 |
| `uprobe` / `uretprobe` | 用户函数 进入/返回 |
| `tracepoint` | 内核预定义的静态跟踪点 |
| `raw_tracepoint` / `tp_btf` | 更高效的 tracepoint（带 BTF）|
| `fentry` / `fexit` | 基于 ftrace 的快速 entry/exit（5.5+，比 kprobe 快） |
| `XDP` | 网络包到达网卡驱动 |
| `tc` | 网络包到达流量控制层 |
| `cgroup_skb` / `cgroup_sock_addr` | cgroup 内的网络事件 |
| `lsm` | Linux Security Module hook（5.7+，BPF LSM）|
| `perf_event` | 采样事件 |

### 5.2 bpftrace —— 一行命令实战

`bpftrace` 是 eBPF 的 awk/perl，用 DSL 写脚本：

```bash
# 安装
sudo apt install bpftrace

# 追踪所有进程打开的文件
sudo bpftrace -e 'tracepoint:syscalls:sys_enter_openat { printf("%s %s\n", comm, str(args->filename)); }'

# 统计每个进程的系统调用频率
sudo bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[comm] = count(); }'

# 追踪 bash 命令历史（uprobe 用户态库函数）
sudo bpftrace -e 'uretprobe:/bin/bash:readline { printf("%s\n", str(retval)); }'
```

适合临时排查、一次性观测。生产部署用下面的 libbpf。

### 5.3 BCC —— Python 框架

[BCC（BPF Compiler Collection）](https://github.com/iovisor/bcc) 把 BPF C 代码嵌在 Python 里：

```python
# trace_open.py
from bcc import BPF

prog = """
TRACEPOINT_PROBE(syscalls, sys_enter_openat) {
    bpf_trace_printk("openat: %s\\n", args->filename);
    return 0;
}
"""

b = BPF(text=prog)
print("Tracing... Ctrl-C to quit.")
b.trace_print()
```

```bash
sudo python3 trace_open.py
```

BCC **每次启动都要 clang 编译**，目标机需要装 clang + 内核头文件，部署不便——这是历史上 eBPF 大规模落地的最大障碍。**CO-RE 就是为解决这个发明的**。

### 5.4 libbpf + CO-RE + BTF —— 现代生产姿势

#### 5.4.1 概念

- **BTF（BPF Type Format）**：把内核所有结构体的类型信息编入内核镜像（`/sys/kernel/btf/vmlinux`）。
- **CO-RE（Compile Once, Run Everywhere）**：BPF 程序里访问内核结构体字段时，编译器记录"在编译用的内核版本上，这个字段在结构体里的偏移"；libbpf 加载时通过目标内核的 BTF **重新计算偏移**并 relocate。
- **libbpf**：纯 C 库，负责加载 BPF、解析 BTF、做 relocation、attach、map 操作。

效果：**编译一次的 .o 文件，可以在任何启用 BTF 的内核上（5.8+ 默认开启）原地运行**，不再需要目标机装 clang/内核头文件。

#### 5.4.2 完整示例：追踪所有 execve 调用

##### 项目结构

```
execve_trace/
  ├── execve.bpf.c          # BPF 程序（运行在内核）
  ├── execve.c              # 用户态加载器
  ├── vmlinux.h             # 由 bpftool 生成
  └── Makefile
```

##### 生成 vmlinux.h

```bash
sudo bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

这个文件几万行，包含全部内核结构体定义。BPF 程序只 include 它即可，不再依赖内核头文件。

##### execve.bpf.c

```c
[[include]] "vmlinux.h"
[[include]] <bpf/bpf_helpers.h>
[[include]] <bpf/bpf_core_read.h>
[[include]] <bpf/bpf_tracing.h>

[[define]] MAX_FILENAME 256

struct event {
    __u32 pid;
    __u32 uid;
    char  comm[16];
    char  filename[MAX_FILENAME];
};

// 用 ring buffer 把事件传给用户态（5.8+，比 perf buffer 高效）
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} events SEC(".maps");

SEC("tp/syscalls/sys_enter_execve")
int handle_execve(struct trace_event_raw_sys_enter *ctx) {
    struct event *e = bpf_ringbuf_reserve(&events, sizeof(*e), 0);
    if (!e) return 0;

    e->pid = bpf_get_current_pid_tgid() >> 32;
    e->uid = bpf_get_current_uid_gid();
    bpf_get_current_comm(&e->comm, sizeof(e->comm));

    // 系统调用第一个参数是 filename 字符串
    const char *filename = (const char *)BPF_CORE_READ(ctx, args[0]);
    bpf_probe_read_user_str(e->filename, sizeof(e->filename), filename);

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
```

要点：

- `SEC("tp/syscalls/sys_enter_execve")` 告诉 libbpf 这个程序 attach 到哪个 tracepoint。
- `BPF_CORE_READ(ctx, args[0])` 是 CO-RE 宏：根据加载时的 BTF 算出 `args[0]` 在 `trace_event_raw_sys_enter` 里的偏移。
- ring buffer 是单生产-多消费、无锁的、按消息边界的内核-用户态通道，替代旧的 perf buffer。

##### execve.c（用户态加载器）

```c
[[include]] <stdio.h>
[[include]] <signal.h>
[[include]] <unistd.h>
[[include]] <bpf/libbpf.h>
[[include]] "execve.skel.h"          // bpftool 自动生成的 skeleton

struct event { __u32 pid, uid; char comm[16], filename[256]; };

static volatile sig_atomic_t exiting = 0;
static void sig_handler(int sig) { exiting = 1; }

static int handle_event(void *ctx, void *data, size_t size) {
    struct event *e = data;
    printf("pid=%u uid=%u comm=%s filename=%s\n", e->pid, e->uid, e->comm, e->filename);
    return 0;
}

int main(void) {
    signal(SIGINT, sig_handler);
    struct execve_bpf *skel = execve_bpf__open_and_load();
    if (!skel) { fprintf(stderr, "open_and_load failed\n"); return 1; }
    if (execve_bpf__attach(skel)) { fprintf(stderr, "attach failed\n"); return 2; }

    struct ring_buffer *rb = ring_buffer__new(bpf_map__fd(skel->maps.events),
                                              handle_event, NULL, NULL);
    while (!exiting) {
        int err = ring_buffer__poll(rb, 100);
        if (err < 0 && err != -EINTR) break;
    }
    ring_buffer__free(rb);
    execve_bpf__destroy(skel);
    return 0;
}
```

##### Makefile

```makefile
CLANG := clang
BPFTOOL := bpftool
ARCH := x86

all: execve

execve.bpf.o: execve.bpf.c vmlinux.h
	$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) \
		-c execve.bpf.c -o execve.bpf.o

execve.skel.h: execve.bpf.o
	$(BPFTOOL) gen skeleton execve.bpf.o > execve.skel.h

execve: execve.c execve.skel.h
	gcc -o execve execve.c -lbpf -lelf -lz

clean:
	rm -f execve execve.bpf.o execve.skel.h
```

##### 构建运行

```bash
sudo apt install clang llvm libelf-dev libbpf-dev linux-tools-common bpftool
make
sudo ./execve
# 另一个终端跑命令，第一个终端会看到:
# pid=12345 uid=1000 comm=bash filename=/usr/bin/ls
```

#### 5.4.3 BPF LSM —— 用 eBPF 做强制访问控制

5.7 内核引入 `BPF_PROG_TYPE_LSM`，可以 attach 到 LSM hook 点，**返回非零阻止操作**。例如禁止任何非 root 进程 mmap PROT_EXEC：

```c
SEC("lsm/mmap_file")
int BPF_PROG(mmap_check, struct file *file, unsigned long prot, ...) {
    if ((prot & PROT_EXEC) && bpf_get_current_uid_gid() != 0) {
        return -EPERM;
    }
    return 0;
}
```

需要内核编译启用 `CONFIG_BPF_LSM=y` 且启动参数 `lsm=...,bpf`。

#### 5.4.4 XDP —— 网卡驱动级 hook

XDP 在网卡驱动收包时第一时间运行 BPF，可以丢弃、转发、修改、放行。Cloudflare 用它做 DDoS 防护，单核可处理几千万 pps。

```c
SEC("xdp")
int xdp_drop_icmp(struct xdp_md *ctx) {
    void *data = (void*)(long)ctx->data;
    void *end  = (void*)(long)ctx->data_end;
    struct ethhdr *eth = data;
    if ((void*)(eth + 1) > end) return XDP_PASS;
    if (eth->h_proto != bpf_htons(ETH_P_IP)) return XDP_PASS;
    struct iphdr *ip = (void*)(eth + 1);
    if ((void*)(ip + 1) > end) return XDP_PASS;
    if (ip->protocol == IPPROTO_ICMP) return XDP_DROP;
    return XDP_PASS;
}
```

attach：`bpftool net attach xdp obj drop.o sec xdp dev eth0`。

---

## 6. 各方案对比

| | LD_PRELOAD | ptrace | uprobe (无 eBPF) | eBPF uprobe | eBPF kprobe/tracepoint | eBPF XDP |
|---|---|---|---|---|---|---|
| 需 root | ✗（除非 setuid） | ✗（同 uid） | ✓ | ✓（或 CAP_BPF） | ✓ | ✓ |
| 需重启目标 | ✓ | ✗ | ✗ | ✗ | – | – |
| 能改返回值 | ✓ | ✓ | ✗（仅观测） | ✗ | ✗ | ✓（包级） |
| 性能开销 | 接近 0 | 高（每次 syscall）| 高 | 低 | 低 | 极低 |
| 内核版本 | 任意 | 任意 | 3.x+ | 4.4+ (基本)，5.8+ (CO-RE) | 同上 | 4.8+ |
| 跨进程 | ✗ | ✗ | ✓ | ✓ | ✓ | – |
| 不能改的 | setuid 程序 | 同 uid 限制 | – | – | – | – |
| 写复杂度 | 低 | 高 | 中 | 中 | 中 | 高 |

---

## 7. 调试与卸载

### 7.1 调试技巧

- `strace -e openat,execve <cmd>`：粗粒度观察 syscall。
- `ltrace <cmd>`：观察库函数调用（基于 ptrace + GOT 篡改），可作为对比基线验证你的 hook 是否生效。
- `LD_DEBUG=libs <cmd>`：看 ld.so 怎么解析符号，能确认 `LD_PRELOAD` 是否生效。
- `bpftool prog list` / `bpftool map dump`：查看当前已加载的 BPF 程序和 map。
- `cat /sys/kernel/debug/tracing/trace_pipe`：BPF 程序里 `bpf_printk` 的输出落在这里。

### 7.2 卸载

| 技术 | 卸载方式 |
|---|---|
| `LD_PRELOAD` | 进程退出后自动消失；运行中不能卸载 |
| ptrace 注入 | `PTRACE_DETACH`；如果注入了 .so，调 `dlclose` 卸 |
| ftrace/kprobe（无 eBPF） | `echo 0 > .../enable`；删除 events |
| eBPF | 程序 detach + close fd 即卸载；进程退出会自动清理（除非用了 BPF_F_LINK_PIN）|

---

## 8. 常见陷阱

| 陷阱 | 表现 | 原因 | 修法 |
|---|---|---|---|
| LD_PRELOAD 对静态链接二进制无效 | hook 不触发 | 静态链接没有动态符号查找 | 改 inline patch / uprobe |
| LD_PRELOAD 对 setuid 程序无效 | 同上 | 安全机制 | 别 hook setuid，或用 uprobe |
| dlsym 在 hook 函数内首次调时炸 | 段错误 | dlsym 内部用 malloc，而 malloc 正在被 hook | constructor 里提前 resolve |
| hook 函数里 printf 导致死循环 | 栈溢出/挂起 | printf 内部又调被 hook 的 malloc/write | thread-local 守卫 |
| ptrace 附加失败 | EPERM | yama ptrace_scope | 改 sysctl 或同父进程 |
| BPF verifier 拒绝 | 无法加载 | 程序里有未被验证安全的指针访问 | 加边界检查 `if (ptr + sizeof(*p) > data_end)` |
| BPF map 大小爆炸 | OOM | 没有清理过期项 | 用 LRU map 或定期清理 |
| CO-RE 字段访问返回 0 | 数据全空 | 用了 `ctx->field` 而非 `BPF_CORE_READ` | 改宏；或确认目标内核存在该字段 |
| bpf_probe_read_user 在 fentry 拿不到数据 | 字符串为空 | 进程可能尚未把数据 fault in | 在 sys_enter tracepoint 拿，或用 bpf_copy_from_user_task |
| uprobe attach 到 inlined 函数 | 找不到符号 | 编译器内联了 | 给目标加 `__attribute__((noinline))` 或 attach 到调用方 |

---

## 9. 参考资料

- *Linux Observability with BPF* (David Calavera, Lorenzo Fontana)
- *Learning eBPF* (Liz Rice)
- libbpf: https://github.com/libbpf/libbpf
- libbpf-bootstrap（最佳脚手架）: https://github.com/libbpf/libbpf-bootstrap
- bpftrace 一行命令手册: https://github.com/bpftrace/bpftrace/blob/master/docs/reference_guide.md
- BCC tutorial: https://github.com/iovisor/bcc/blob/master/docs/tutorial.md
- ld.so(8) man page —— 必读，把符号搜索顺序、`LD_*` 全部环境变量讲清楚
- Brendan Gregg eBPF 资源页: https://www.brendangregg.com/ebpf.html

下一篇：[04-language-runtime-hook.md](./04-language-runtime-hook.md)
