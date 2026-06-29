---
aliases: [RISC-V汇编, RISC-V指令集, RV32, RV64, RV64GC]
tags: [汇编/RISC-V, 指令集, 计算机体系结构, 开放架构]
---

# RISC-V 指令集详解：RV32 与 RV64 对比

## 概述

RISC-V 是一种开放、免版税的精简指令集架构（RISC），由加州大学伯克利分校发起，现由 RISC-V International 维护。它采用「基础指令集 + 模块化扩展」的设计：一个最小的基础整数指令集（I）保证通用性，其余功能（乘除、浮点、原子、向量等）以可选扩展形式叠加。

两个主要的基础变体是：
- **RV32I**：32 位地址空间，寄存器宽度（XLEN）= 32
- **RV64I**：64 位地址空间，XLEN = 64

> 命名提示：`RV` 后的数字（XLEN）表示**寄存器宽度**，而不是指令长度——基础指令始终是 32 位定长（C 扩展引入 16 位压缩指令）。

## 寄存器

### 整数寄存器
RISC-V 有 32 个通用整数寄存器 x0–x31（宽度为 XLEN）：

| 寄存器 | ABI 名 | 用途 | 调用约定 |
|--------|--------|------|---------|
| x0 | zero | 硬连线常数 0（写入被忽略） | — |
| x1 | ra | 返回地址 | Caller-saved |
| x2 | sp | 栈指针 | Callee-saved |
| x3 | gp | 全局指针 | 不可分配 |
| x4 | tp | 线程指针 | 不可分配 |
| x5–x7 | t0–t2 | 临时寄存器 | Caller-saved |
| x8 | s0/fp | 保存寄存器 / 帧指针 | Callee-saved |
| x9 | s1 | 保存寄存器 | Callee-saved |
| x10–x11 | a0–a1 | 函数参数 / 返回值 | Caller-saved |
| x12–x17 | a2–a7 | 函数参数 | Caller-saved |
| x18–x27 | s2–s11 | 保存寄存器 | Callee-saved |
| x28–x31 | t3–t6 | 临时寄存器 | Caller-saved |

栈向下增长，且 `sp` 始终保持 16 字节对齐。

### 浮点寄存器（F/D 扩展）
启用浮点扩展后，新增 32 个浮点寄存器 f0–f31：
- ABI 名：`ft0–ft11`（临时）、`fs0–fs11`（保存）、`fa0–fa7`（参数/返回值）
- 单精度（F）用低 32 位，双精度（D）用全 64 位（NaN-boxing 规则保证单精度值在 64 位寄存器中的表示）

### 浮点控制状态寄存器 fcsr
- `fcsr` 包含 `frm`（舍入模式）和 `fflags`（累积异常标志）
- 浮点比较结果写入**整数寄存器**（而非专用标志），与 LoongArch 写 fcc、x86 写 EFLAGS 不同

## 指令格式

RISC-V 基础指令为 32 位定长，共有 6 种核心编码格式：

| 格式 | 用途 | 典型指令 |
|------|------|---------|
| R 型 | 寄存器-寄存器运算 | `add`, `sub`, `sll` |
| I 型 | 立即数运算 / 加载 / `jalr` | `addi`, `lw`, `jalr` |
| S 型 | 存储 | `sw`, `sd` |
| B 型 | 条件分支 | `beq`, `blt` |
| U 型 | 高位立即数 | `lui`, `auipc` |
| J 型 | 无条件跳转 | `jal` |

设计上各格式的寄存器字段（rs1/rs2/rd）位置尽量固定，立即数虽被打散到不同位段，但符号位始终在最高位，简化了硬件立即数提取与符号扩展。

## 基础整数指令

### 算术与逻辑（R 型 / I 型）
- `add rd, rs1, rs2` / `addi rd, rs1, imm`：加法
- `sub rd, rs1, rs2`：减法（无 `subi`，用 `addi` 加负数）
- `and` / `or` / `xor`（及 `andi` / `ori` / `xori`）：按位逻辑
- `slt rd, rs1, rs2` / `slti`：有符号小于置 1
- `sltu rd, rs1, rs2` / `sltiu`：无符号小于置 1
- `lui rd, imm`：加载 20 位立即数到 rd 的高位（[31:12]）
- `auipc rd, imm`：PC + (imm<<12)，用于位置无关寻址

### 移位
- `sll` / `slli`：逻辑左移
- `srl` / `srli`：逻辑右移
- `sra` / `srai`：算术右移
- 移位量：RV32 取低 5 位，RV64 取低 6 位

### RV64I 新增的 W（字）指令
RV64 寄存器为 64 位，但为高效处理 32 位 `int`，提供一组 `*w` 指令——它们只对低 32 位运算，并把 32 位结果**符号扩展**回 64 位：
- `addw` / `subw` / `addiw`
- `sllw` / `srlw` / `sraw`（及 `slliw` / `srliw` / `sraiw`）

> `sext.w rd, rs` 是 `addiw rd, rs, 0` 的伪指令，用于把低 32 位符号扩展到 64 位。

## 无标志位的条件判断

与 MIPS、LoongArch 一样，RISC-V **没有标志位寄存器**（无 CF/ZF/SF/OF）。但它比 MIPS 更进一步：分支指令可以**直接比较两个寄存器**，无需先用比较指令生成布尔值。

```asm
# RISC-V：一条指令完成比较 + 分支
blt t0, t1, label    # 若 t0 < t1（有符号）则跳转

# 对比 MIPS：需要两条
# slt t2, t0, t1
# bne t2, zero, label
```

`slt`/`sltu` 仍用于需要把比较结果存为 0/1 的场景（如实现 `a < b ? 1 : 0`）。

## 访存指令

RISC-V 是典型的 load/store 架构，仅 load/store 指令访问内存。

| 操作 | RV32I | RV64I 新增 |
|------|-------|-----------|
| 加载字节/半字（符号扩展） | `lb` / `lh` | — |
| 加载字节/半字（零扩展） | `lbu` / `lhu` | — |
| 加载字 | `lw`（符号扩展到 XLEN） | `lwu`（零扩展到 64 位） |
| 加载双字 | — | `ld` |
| 存储 | `sb` / `sh` / `sw` | `sd` |

寻址方式单一：`offset(rs1)`，即基址寄存器 + 12 位有符号立即数偏移。

```asm
lw   a0, 8(sp)       # 从 sp+8 加载 32 位字（RV64 下符号扩展到 64 位）
sd   a1, 0(t0)       # 把 a1 的 64 位值存到 t0 指向的地址
```

## 分支与跳转

### 条件分支（B 型）
- `beq` / `bne`：相等 / 不等
- `blt` / `bge`：有符号小于 / 大于等于
- `bltu` / `bgeu`：无符号小于 / 大于等于
- `bgt`/`ble`/`bgtu`/`bleu` 是交换操作数实现的伪指令

### 无条件跳转
- `jal rd, offset`：跳转并把返回地址存入 rd（通常 rd=ra）
- `jalr rd, rs1, offset`：跳转到 rs1+offset，返回地址存入 rd
- `j offset` / `jr rs1` / `ret` 是常用伪指令（分别基于 jal/jalr）

> RISC-V **没有分支延迟槽**（这是相对 MIPS 的重要简化）。

## M 扩展：整数乘除

- `mul rd, rs1, rs2`：乘积低 XLEN 位
- `mulh` / `mulhu` / `mulhsu`：乘积高 XLEN 位（有符号×有符号 / 无符号×无符号 / 有符号×无符号）
- `div` / `divu`：有符号 / 无符号除法
- `rem` / `remu`：有符号 / 无符号取余
- RV64 新增：`mulw`、`divw`、`divuw`、`remw`、`remuw`（对低 32 位运算）

## F / D 扩展：浮点

### 加载/存储
- `flw` / `fsw`：单精度加载/存储
- `fld` / `fsd`：双精度加载/存储

### 算术（`.s` 单精度，`.d` 双精度）
- `fadd.s` / `fsub.s` / `fmul.s` / `fdiv.s` / `fsqrt.s`
- `fmin.s` / `fmax.s`：最小/最大值
- 融合乘加：`fmadd.s` / `fmsub.s` / `fnmadd.s` / `fnmsub.s`
- 符号注入：`fsgnj.s`（拷贝符号）/ `fsgnjn.s`（取反符号）/ `fsgnjx.s`（异或符号，可实现 abs/neg）

### 比较（结果写整数寄存器）
- `feq.s rd, rs1, rs2`：相等
- `flt.s rd, rs1, rs2`：小于
- `fle.s rd, rs1, rs2`：小于等于

### 转换与传送
- `fcvt.w.s` / `fcvt.wu.s`：单精度 → 32 位有符号/无符号整数
- `fcvt.l.s` / `fcvt.lu.s`：单精度 → 64 位整数（RV64）
- `fcvt.s.w` / `fcvt.s.wu`：整数 → 单精度
- `fcvt.s.d` / `fcvt.d.s`：单双精度互转
- `fmv.x.w` / `fmv.w.x`：在整数与浮点寄存器间按位传送

## A 扩展：原子操作

- `lr.w` / `sc.w`：加载保留 / 条件存储（RV64 另有 `lr.d` / `sc.d`）
- `amoadd.w` / `amoswap.w` / `amoand.w` / `amoor.w` / `amoxor.w` / `amomin.w` / `amomax.w` 等原子内存操作（均有 `.d` 版本）
- 配合 `fence` 指令控制内存访问顺序

## C 扩展：压缩指令

C 扩展引入 16 位压缩指令（如 `c.addi`、`c.lw`、`c.sw`、`c.beqz`、`c.jr`），与 32 位指令自由混合，可显著减小代码体积。常见于嵌入式与对代码密度敏感的场景。

## 扩展命名与 G

标准扩展以单字母后缀追加在基础 ISA 后：

| 字母 | 含义 |
|------|------|
| I | 基础整数（必选） |
| M | 整数乘除 |
| A | 原子操作 |
| F | 单精度浮点 |
| D | 双精度浮点 |
| C | 压缩（16 位）指令 |

由于 **IMAFD** 组合极为常用，约定用 **G**（General）作为其简写。因此 **RV64GC = RV64IMAFDC**，是当前通用软件栈最常见的目标。

## 特权级与系统指令

RISC-V 定义三种特权模式：
- **M-mode（机器模式）**：最高特权，固件 / SBI 运行于此
- **S-mode（监督模式）**：操作系统内核（可选，支持分页与虚拟内存）
- **U-mode（用户模式）**：应用程序

系统/CSR 指令：
- `ecall`：环境调用（系统调用 / 陷入更高特权级）
- `ebreak`：断点
- `csrrw` / `csrrs` / `csrrc`（及 `csrrwi` / `csrrsi` / `csrrci`）：读-写/置位/清位控制状态寄存器
- `fence` / `fence.i`：内存 / 取指屏障

## RV32 与 RV64 主要差异

| 特性 | RV32I | RV64I |
|------|-------|-------|
| 寄存器宽度（XLEN） | 32 位 | 64 位 |
| 地址空间 | 4GB | 理论 2^64 |
| 移位量位数 | 5 位 | 6 位 |
| 新增运算指令 | — | `addw`/`subw`/`sllw`/`srlw`/`sraw`（及 i 版本） |
| 访存新增 | — | `ld`/`sd`/`lwu` |
| `lw` 行为 | 加载 32 位 | 符号扩展到 64 位 |

## 与主流架构的对比

### 与 MIPS 对比
- 同为经典 RISC，但 RISC-V **分支可直接比较两寄存器**（`blt`/`bge`），不必先 `slt`
- RISC-V **无分支延迟槽**，控制流更直观
- 模块化扩展替代了 MIPS 的协处理器概念

### 与 LoongArch 对比
- 都是现代、无整数标志位的 RISC，模块化设计
- 浮点比较：RISC-V 写**整数寄存器**，LoongArch 写专用 **fcc** 条件标志
- LoongArch 指令格式更收敛（9 种），RISC-V 核心 6 种格式

### 与 ARM 对比
- 无条件执行、无标志位（AArch64 有 NZCV）
- 完全开放、免版税 vs ARM 的授权模式
- 同为定长 32 位编码（RISC-V 的 C 扩展提供 16 位压缩）

### 与 x86-64 对比
- 定长、load/store、寻址单一 vs x86 变长、复杂寻址、读改写内存操作数
- 无标志位 vs x86 丰富的 EFLAGS
- 精简正交 vs 历史包袱叠加

## 总结

RISC-V 的核心价值在于「**开放 + 模块化 + 简洁**」：

- **开放免费**：任何人可自由实现，无授权费，催生了庞大的学术与产业生态。
- **基础 + 扩展**：RV32I/RV64I 极小且稳定，M/A/F/D/C/V 等扩展按需叠加，`G` 是通用组合。
- **彻底的 RISC**：无标志位、无延迟槽、load/store、定长编码，分支直接比较寄存器。
- **面向未来**：预留了向量（V）、虚拟化（H）等扩展空间。

凭借开放性与可裁剪性，RISC-V 已广泛用于嵌入式、加速器、教学乃至高性能计算领域。

## 相关架构

- [[21.Asm/11 ARM.md|ARM 汇编指令详解]]
- [[21.Asm/12 MIPS.md|MIPS 汇编指令详解]]
- [[21.Asm/13 LoongArch64.md|LoongArch64 指令集详解]]
- [[21.Asm/15 x86(64).md|x86 汇编指令详解]]
- [[21.Asm/01 汇编指令集对比.md|五架构对比总览]]
