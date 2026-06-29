---
aliases: [HelloWorld汇编, 汇编示例, Hello World示例, 五架构示例]
tags: [汇编, 指令集, 示例代码, Linux, 计算机体系结构, MOC]
---

# 各架构 Hello World 汇编示例

本页给出五大架构在 **Linux 用户态**下、**不依赖 C 库**（直接系统调用）的 Hello World 完整汇编，采用 GNU as（GAS）语法。每段都附编译/链接/运行命令。

> 说明：系统调用号是 **Linux 特定**的（裸机或其他 OS 不同）。下列代码依据已核实的 Linux syscall 表与各架构汇编语法编写；x86-64 可在本机 Linux 直接运行，其余架构建议用对应交叉工具链 + QEMU 用户态模拟运行。

## 系统调用速览

| 架构 | 陷入指令 | 号寄存器 | 参数寄存器 | `write` | `exit` | 返回 |
|------|---------|---------|-----------|---------|--------|------|
| x86-64 | `syscall` | `rax` | rdi, rsi, rdx, r10, r8, r9 | 1 | 60 | rax |
| ARM64 | `svc #0` | `x8` | x0–x5 | 64 | 93 | x0 |
| RISC-V | `ecall` | `a7` | a0–a5 | 64 | 93 | a0 |
| LoongArch | `syscall 0` | `a7` | a0–a6 | 64 | 93 | a0 |
| MIPS o32 | `syscall` | `v0` | a0–a3 | 4004 | 4001 | v0（a3=错误位） |

> ARM64 / RISC-V / LoongArch 采用 “asm-generic” 统一编号（write=64、exit=93）；x86-64 与 MIPS 各有历史编号。

## x86-64

```asm
# hello-x86_64.s — Linux x86-64（write=1, exit=60，号在 rax）
.section .data
msg:    .ascii "Hello, World!\n"
        len = . - msg

.section .text
.global _start
_start:
    mov     $1, %rax            # __NR_write = 1
    mov     $1, %rdi            # fd = stdout
    lea     msg(%rip), %rsi     # buf
    mov     $len, %rdx          # count
    syscall

    mov     $60, %rax           # __NR_exit = 60
    xor     %rdi, %rdi          # status = 0
    syscall
```

```bash
# 本机 Linux 直接编译运行：
gcc -nostdlib -static -no-pie -o hello hello-x86_64.s && ./hello
# 或仅用 binutils：
as -o hello.o hello-x86_64.s && ld -o hello hello.o && ./hello
```

## ARM64（AArch64）

```asm
// hello-aarch64.s — Linux ARM64（write=64, exit=93，号在 x8）
.section .data
msg:    .ascii "Hello, World!\n"
        len = . - msg

.section .text
.global _start
_start:
    mov     x0, #1             // fd = stdout
    ldr     x1, =msg           // buf
    mov     x2, #len           // count
    mov     x8, #64            // __NR_write = 64
    svc     #0

    mov     x0, #0             // status = 0
    mov     x8, #93            // __NR_exit = 93
    svc     #0
```

```bash
aarch64-linux-gnu-gcc -nostdlib -static -o hello hello-aarch64.s
qemu-aarch64 ./hello
```

## RISC-V（RV64）

```asm
# hello-riscv64.s — Linux RISC-V（write=64, exit=93，号在 a7）
.section .data
msg:    .ascii "Hello, World!\n"
        len = . - msg

.section .text
.global _start
_start:
    li      a0, 1              # fd = stdout
    la      a1, msg            # buf
    li      a2, len            # count
    li      a7, 64             # __NR_write = 64
    ecall

    li      a0, 0              # status = 0
    li      a7, 93             # __NR_exit = 93
    ecall
```

```bash
riscv64-linux-gnu-gcc -nostdlib -static -o hello hello-riscv64.s
qemu-riscv64 ./hello
```

## LoongArch64

```asm
# hello-loongarch64.s — Linux LoongArch（write=64, exit=93，号在 a7）
.section .data
msg:    .ascii "Hello, World!\n"
        len = . - msg

.section .text
.global _start
_start:
    li.d        $a0, 1         # fd = stdout
    la.local    $a1, msg       # buf
    li.d        $a2, len       # count
    li.d        $a7, 64        # __NR_write = 64
    syscall     0

    li.d        $a0, 0         # status = 0
    li.d        $a7, 93        # __NR_exit = 93
    syscall     0
```

```bash
loongarch64-linux-gnu-gcc -nostdlib -static -o hello hello-loongarch64.s
qemu-loongarch64 ./hello
```

## MIPS（o32，大端）

```asm
# hello-mips.s — Linux MIPS o32（write=4004, exit=4001，号在 v0）
.section .data
msg:    .ascii "Hello, World!\n"
        len = . - msg

.section .text
.global _start
_start:
    li      $a0, 1             # fd = stdout
    la      $a1, msg           # buf
    li      $a2, len           # count
    li      $v0, 4004          # __NR_write (o32) = 4004
    syscall

    li      $a0, 0             # status = 0
    li      $v0, 4001          # __NR_exit (o32) = 4001
    syscall
```

```bash
# 大端 MIPS：
mips-linux-gnu-gcc -nostdlib -static -o hello hello-mips.s
qemu-mips ./hello
# 小端则用 mipsel-linux-gnu-gcc + qemu-mipsel
```

## 要点对照

- **加载字符串地址**：x86-64 用 `lea ...(%rip)`（RIP 相对）；ARM64 用 `ldr x1, =msg`（字面量池）；RISC-V `la`、LoongArch `la.local`、MIPS `la` 都是地址加载伪指令（展开为 PC 相对的两条指令）。
- **立即数加载**：RISC-V/MIPS 用 `li`，LoongArch 用 `li.d`，x86/ARM 直接 `mov`。
- **`len = . - msg`**：用「当前地址 - 标号」在汇编期算出字符串长度，五架构通用。
- **退出码**：现代架构（ARM64/RISC-V/LoongArch）实际更推荐 `exit_group`（号 94）；单线程程序用 `exit`（93）亦可正常退出。

## 相关页面

- [[21.Asm/27 SIMD实战示例.md|SIMD 实战示例：多架构向量化代码]]
- [[21.Asm/29 异常中断处理对比.md|异常 / 中断处理对比]]
- [[21.Asm/28 原子操作与内存模型.md|原子操作与内存模型对比]]
- [[21.Asm/23 调用约定对比.md|调用约定 / ABI 横向对照]]
- [[21.Asm/01 汇编指令集对比.md|五架构对比总览]]
- [[21.Asm/11 ARM.md|ARM]]｜[[21.Asm/12 MIPS.md|MIPS]]｜[[21.Asm/13 LoongArch64.md|LoongArch]]｜[[21.Asm/14 RISC-V.md|RISC-V]]｜[[21.Asm/15 x86(64).md|x86]]
