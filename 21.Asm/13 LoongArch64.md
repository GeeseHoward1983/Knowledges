---
aliases: [LoongArch, 龙芯架构, LoongArch64, LoongArch32]
tags: [汇编/LoongArch, 指令集, 计算机体系结构, 国产架构]
---

# LoongArch64 指令集详解

## 简介

LoongArch 是龙芯中科推出的一种全新的精简指令集架构（RISC），它不是对现有指令系统的扩展，而是一种从零开始设计的全新指令集。LoongArch 架构具有完全自主知识产权，其指令集规范完全开放，旨在构建一个开放式的技术生态。

LoongArch 架构分为三个子架构：
- LoongArch32：32 位版本
- LoongArch64：64 位版本
- LoongArch64S：简化版 64 位版本

本文档主要介绍 LoongArch64 指令集架构，同时也会涉及 LoongArch32 的对比分析。

## 架构特点

### 设计哲学
- **简洁性**：指令格式规整，寻址方式简单，减少硬件复杂度
- **模块化**：基础指令集 + 扩展指令集的模块化设计
- **高效性**：采用定长指令编码，便于流水线设计
- **可扩展性**：预留大量指令编码空间以支持未来功能扩展
- **开放性**：指令集规范公开，鼓励生态建设

### 寄存器组织

LoongArch64 拥有 32 个 64 位通用寄存器（GPR），记作 r0-r31：

| 寄存器 | ABI 名称 | 描述 |
|--------|---------|------|
| r0 | $zero | 零寄存器，读取恒为 0，写入被忽略 |
| r1 | $ra | 返回地址 |
| r2 | $tp | 线程指针 |
| r3 | $sp | 栈指针 |
| r4-r7 | $a0-$a3 | 函数调用参数/返回值 |
| r8-r11 | $a4-$a7 | 函数调用参数 |
| r12-r20 | $t0-$t8 | 临时寄存器 |
| r21 | 保留 | 保留 |
| r22 | $fp/$s9 | 帧指针 |
| r23-r31 | $s0-$s8 | 被调用者保存寄存器 |

> ✅ 已核对：本表与官方《LoongArch ELF psABI / lapcs》一致——$t0–$t8 为 r12–r20（caller-saved），r21 保留（Linux 内核用作 percpu 基址，名 $u0），$fp/$s9 为 r22，$s0–$s8 为 r23–r31（callee-saved）。

LoongArch32 拥有 32 个 32 位通用寄存器（GPR），记作 r0-r31，与 LoongArch64 类似，但寄存器宽度为 32 位。

### LoongArch32 与 LoongArch64 对比

| 特性 | LoongArch32 | LoongArch64 |
|------|-------------|-------------|
| 寄存器宽度 | 32 位 | 64 位 |
| 虚拟地址宽度 | 32 位 | 48 位（物理地址最多 56 位） |
| 地址空间大小 | 4GB | 256TB |
| 指令编码 | 固定 32 位 | 固定 32 位 |
| 基本整数指令 | add.w, sub.w | add.w, sub.w, add.d, sub.d |
| 访存指令 | ld.w, st.w | ld.w, st.w, ld.d, st.d |

虽然两者的基本指令格式相同，但 LoongArch64 增加了 64 位操作指令，而 LoongArch32 仅支持 32 位操作。

### 特权等级

LoongArch 定义了四个特权等级 PLV0–PLV3，**数字越小特权越高**：
- PLV0：最高特权级，操作系统内核运行于此
- PLV1、PLV2：中间特权级（常用于虚拟化或特定系统软件，多数系统中保留）
- PLV3：最低特权级，用户态应用程序运行于此

> 订正：原文将特权级描述为 “0 级 User / 3 级 Machine 最高”，与 LoongArch 实际定义相反，已更正为 PLV0 最高、PLV3 最低（已与官方手册比对）。

## 指令格式

LoongArch 采用固定长度的 32 位指令编码，官方共定义 **9 种基本指令格式**：3 种无立即数（2R、3R、4R）和 6 种含立即数（2RI8、2RI12、2RI14、2RI16、1RI21、I26）。编码风格统一——寄存器字段从第 0 位起由低到高排列，opcode 从第 31 位起由高到低排列，立即数字段位于二者之间。

| 格式 | 组成 | 典型用途/示例 |
|------|------|--------------|
| 2R | opcode + rj + rd | 一元寄存器操作 |
| 3R | opcode + rk + rj + rd | 寄存器-寄存器运算，如 `add.d rd, rj, rk` |
| 4R | opcode + ra + rk + rj + rd | 融合乘加类，如 `fmadd.d` |
| 2RI8 | opcode + I8 + rj + rd | 带 8 位立即数 |
| 2RI12 | opcode + I12 + rj + rd | 立即数运算/访存，如 `addi.d`、`ld.d rd, rj, si12` |
| 2RI14 | opcode + I14 + rj + rd | `ll.w`/`sc.w` 等 |
| 2RI16 | opcode + I16 + rj + rd | 双寄存器分支，如 `beq rj, rd, offs16` |
| 1RI21 | opcode + I21 + rj | 单寄存器分支，如 `beqz rj, offs21` |
| I26 | opcode + I26 | 长跳转，如 `b offs26`、`bl offs26` |

其中 rd 为目的寄存器，rj/rk/ra 为源寄存器（a = additional）。少数指令的编码不完全等同于这 9 种典型格式，但数量很少。

> 说明：LoongArch 寄存器在汇编中常写作 `rj`/`rk`/`rd`（源 1/源 2/目的）。本文为保持与 ARM/MIPS 篇一致，部分示例沿用 `rs1`/`rs2` 记法，含义相同。

LoongArch32 的指令格式与 LoongArch64 完全一致，但由于只处理 32 位数据，某些指令的操作数范围和结果处理方式会有所不同。

## 基础指令集

### 整数运算指令

#### 算术指令
- `add.w rd, rs1, rs2`：32 位加法，结果符号扩展至目标寄存器宽度
- `add.d rd, rs1, rs2`：64 位加法（仅 LoongArch64）
- `sub.w rd, rs1, rs2`：32 位减法
- `sub.d rd, rs1, rs2`：64 位减法（仅 LoongArch64）
- `slt rd, rs1, rs2`：设置小于（有符号）
- `sltu rd, rs1, rs2`：设置小于（无符号）

### 标志位与条件判断

与 ARM 和 x86 不同，LoongArch 没有传统的标志位寄存器（如 ARM 的 CPSR 或 x86 的 EFLAGS）。LoongArch 采用了一种不同的方法来处理整数条件判断：

- 不像 ARM 那样有 N(负数)、Z(零)、C(进位)、V(溢出)标志位
- 与 MIPS 类似，依赖比较指令产生布尔结果，再配合条件分支

LoongArch 使用显式的比较指令产生布尔结果到通用寄存器：

- `slt rd, rs1, rs2`：如果 rs1 < rs2（有符号），则 rd = 1，否则 rd = 0
- `sltu rd, rs1, rs2`：无符号比较，如果 rs1 < rs2，则 rd = 1，否则 rd = 0
- `maskeqz rd, rs1, rs2`：当 rs2 等于 0 时 rd = 0，否则 rd = rs1
- `masknez rd, rs1, rs2`：当 rs2 不等于 0 时 rd = 0，否则 rd = rs1

这些指令使得条件操作可以直接基于寄存器值进行，无需依赖隐式的标志位。

> ✅ 已核对：`seleqz`/`selnez` 是 MIPS R6 助记符；LoongArch 的等价指令是 `maskeqz`/`masknez`（手册 §2.2.3.10），原文误用了 MIPS 名称，已更正。

### 逻辑指令
- `and rd, rs1, rs2`：按位与
- `or rd, rs1, rs2`：按位或
- `xor rd, rs1, rs2`：按位异或
- `nor rd, rs1, rs2`：按位或非

### 移位指令
- `slli.w rd, rs1, imm`：32 位逻辑左移
- `slli.d rd, rs1, imm`：64 位逻辑左移（仅 LoongArch64）
- `srli.w rd, rs1, imm`：32 位逻辑右移
- `srli.d rd, rs1, imm`：64 位逻辑右移（仅 LoongArch64）
- `srai.w rd, rs1, imm`：32 位算术右移
- `srai.d rd, rs1, imm`：64 位算术右移（仅 LoongArch64）

### 立即数指令

#### 加载立即数
- `li.w rd, imm` / `li.d rd, imm`：加载立即数（伪指令，由 lu12i.w 等组合实现）
- `lu12i.w rd, imm`：将 20 位立即数加载到 rd 的 [31:12] 位，低 12 位清零

#### 地址计算
- `pcaddi rd, imm`：rd = PC + (imm << 2)，用于 PC 相对寻址
- `pcaddu12i rd, imm`：rd = PC + (imm << 12)，常与 addi 组合计算地址

### 访存指令

#### 加载指令
- `ld.w rd, rs1, offset`：加载 32 位字，符号扩展
- `ld.d rd, rs1, offset`：加载 64 位双字（仅 LoongArch64）
- `ld.b rd, rs1, offset`：加载 8 位字节，符号扩展
- `ld.h rd, rs1, offset`：加载 16 位半字，符号扩展
- `ld.bu rd, rs1, offset`：加载 8 位字节，零扩展
- `ld.hu rd, rs1, offset`：加载 16 位半字，零扩展
- `ld.wu rd, rs1, offset`：加载 32 位字，零扩展（仅 LoongArch64）

#### 存储指令
- `st.w rs2, rs1, offset`：存储 32 位字
- `st.d rs2, rs1, offset`：存储 64 位双字（仅 LoongArch64）
- `st.b rs2, rs1, offset`：存储 8 位字节
- `st.h rs2, rs1, offset`：存储 16 位半字

### 分支跳转指令

#### 无条件跳转
- `b offset`：无条件 PC 相对跳转
- `bl offset`：跳转并把返回地址写入 $ra（函数调用）
- `jirl rd, rs1, offset`：跳转到 rs1 + offset，并把返回地址写入 rd
- `jr rs1`：寄存器间接跳转（伪指令，等价于 `jirl $zero, rs1, 0`）

#### 条件分支
- `beqz rs1, offset`：若 rs1 等于 0 则分支
- `bnez rs1, offset`：若 rs1 不等于 0 则分支
- `beq rs1, rs2, offset`：若 rs1 等于 rs2 则分支
- `bne rs1, rs2, offset`：若 rs1 不等于 rs2 则分支
- `blt rs1, rs2, offset`：若 rs1 < rs2 则分支（有符号）
- `bge rs1, rs2, offset`：若 rs1 >= rs2 则分支（有符号）
- `bltu rs1, rs2, offset`：若 rs1 < rs2 则分支（无符号）
- `bgeu rs1, rs2, offset`：若 rs1 >= rs2 则分支（无符号）

> 说明：`bgt`/`ble`/`bgtu`/`bleu` 是汇编器伪指令，通过交换操作数映射到 `blt`/`bge`/`bltu`/`bgeu`（例如 `bgt a, b` 等价于 `blt b, a`）。硬件实际只有 `blt`/`bge`/`bltu`/`bgeu`。

### 浮点数相关指令

LoongArch 具有专门的浮点指令集扩展，包括 32 个浮点寄存器（f0-f31），支持 IEEE 754 标准的单精度和双精度运算。

#### 浮点寄存器详解
- f0-f31：32 个浮点寄存器（基础浮点为 64 位；启用 LSX/LASX 向量扩展时与 128/256 位向量寄存器共享低位）
- 每个寄存器可存储：
  - 一个双精度浮点数（64 位）
  - 一个单精度浮点数（32 位，存于低 32 位）

#### 浮点条件标志与控制寄存器
- 浮点比较结果写入 8 个浮点条件标志 fcc0-fcc7（而非通用寄存器）
- FCSR（浮点控制状态寄存器）管理浮点运算的异常和舍入模式，包括舍入模式控制位、异常使能位、异常标志位等

#### 浮点数据传输指令
- `fld.s fd, rs1, offset`：从内存加载单精度浮点数到浮点寄存器
- `fst.s fd, rs1, offset`：将单精度浮点数从浮点寄存器存储到内存
- `fld.d fd, rs1, offset`：从内存加载双精度浮点数到浮点寄存器
- `fst.d fd, rs1, offset`：将双精度浮点数从浮点寄存器存储到内存
- `fmov.s fd, fs` / `fmov.d fd, fs`：浮点寄存器间传送
- `movgr2fr.d fd, rj` / `movfr2gr.d rd, fj`：在通用寄存器与浮点寄存器间传送

#### 浮点算术运算指令
- `fadd.s fd, fs1, fs2`：单精度浮点加法
- `fadd.d fd, fs1, fs2`：双精度浮点加法
- `fsub.s fd, fs1, fs2`：单精度浮点减法
- `fsub.d fd, fs1, fs2`：双精度浮点减法
- `fmul.s fd, fs1, fs2`：单精度浮点乘法
- `fmul.d fd, fs1, fs2`：双精度浮点乘法
- `fdiv.s fd, fs1, fs2`：单精度浮点除法
- `fdiv.d fd, fs1, fs2`：双精度浮点除法
- `fsqrt.s fd, fs1`：单精度浮点平方根
- `fsqrt.d fd, fs1`：双精度浮点平方根
- `fabs.s fd, fs1`：单精度浮点绝对值
- `fabs.d fd, fs1`：双精度浮点绝对值
- `fneg.s fd, fs1`：单精度浮点取反
- `fneg.d fd, fs1`：双精度浮点取反

#### 浮点比较指令
- `fcmp.cond.s fcc, fs1, fs2`：单精度浮点比较，结果写入浮点条件标志 fcc
- `fcmp.cond.d fcc, fs1, fs2`：双精度浮点比较，结果写入浮点条件标志 fcc
- 其中 cond 可以是：`.cun`(无序)、`.ceq`(相等)、`.cle`(小于等于)、`.clt`(小于)等
- 分支时配合 `bceqz fcc, offset` / `bcnez fcc, offset` 使用

#### 浮点类型转换指令
- `fcvt.s.d fd, fs`：双精度转单精度
- `fcvt.d.s fd, fs`：单精度转双精度

整数 ↔ 浮点转换（`ffint` = 整数转浮点，`ftint` = 浮点转整数）：
- `ffint.s.w fd, fs`：32 位整数转单精度浮点
- `ffint.d.w fd, fs`：32 位整数转双精度浮点
- `ffint.s.l fd, fs`：64 位整数转单精度浮点
- `ffint.d.l fd, fs`：64 位整数转双精度浮点
- `ftintrz.w.s fd, fs`：单精度浮点转 32 位整数（向零舍入）
- `ftintrz.l.s fd, fs`：单精度浮点转 64 位整数（向零舍入）
- `ftintrz.w.d fd, fs`：双精度浮点转 32 位整数（向零舍入）
- `ftintrz.l.d fd, fs`：双精度浮点转 64 位整数（向零舍入）
- `frint.s fd, fs` / `frint.d fd, fs`：按 FCSR 当前舍入模式舍入到整数值（结果仍为浮点格式）

> ✅ 已核对：整数→浮点为 `ffint.{s/d}.{w/l}`；浮点→整数为 `ftint{rm/rp/rz/rne}.{w/l}.{s/d}`（rm=向负无穷、rp=向正无穷、rz=向零、rne=向最近偶数），以及按 FCSR 当前模式的 `ftint.{w/l}.{s/d}`。原文的 `flt.*`/`ftrint.*` 助记符有误，已更正（已与 LoongArch Vol.1 及 QEMU 翻译实现比对）。

LoongArch 的浮点指令遵循 IEEE 754 标准，支持多种舍入模式（向零舍入、向正无穷舍入、向负无穷舍入、向最近偶数舍入），与 ARM 和 MIPS 的浮点处理方式相似，但指令命名更加一致。

### 同步指令

- `ll.w rd, rs1, offset`：32 位加载链接
- `sc.w rd, rs1, offset`：32 位存储条件
- `ll.d rd, rs1, offset`：64 位加载链接（仅 LoongArch64）
- `sc.d rd, rs1, offset`：64 位存储条件（仅 LoongArch64）
- `dbar hint`：数据屏障
- `ibar hint`：指令屏障

### 系统指令

- `syscall`：系统调用
- `break`：软件断点
- `csrrd rd, csr`：读控制状态寄存器
- `csrwr rd, csr`：写控制状态寄存器
- `csrxchg rd, rj, csr`：按掩码交换控制状态寄存器

## 扩展指令集

### LSX（Loongson SIMD eXtension）—— 128 位向量扩展
LSX 提供 128 位向量处理能力，支持 16 个字节、8 个半字、4 个字或 4 个单精度浮点数等的并行操作。

### LASX（Loongson Advanced SIMD eXtension）—— 256 位向量扩展
LASX 提供 256 位向量处理能力，是 LSX 的超集，支持更宽的向量操作。

### LVZ（Loongson Virtualization）—— 虚拟化扩展
LVZ 提供硬件虚拟化支持（如客户机/宿主机上下文、虚拟化相关的 CSR 与异常机制），与向量运算无关。

> 订正：原文把 LSX 误标为 “虚拟化扩展”、把 LVZ 误标为 “向量化压缩扩展”；三者职责已更正：LSX/LASX 为向量（SIMD）扩展，LVZ 为虚拟化扩展（已与官方手册比对）。

### LSX / LASX 向量指令代表性示例

#### 寄存器命名

| 扩展 | 向量宽度 | 寄存器名 | 与浮点寄存器关系 |
|------|---------|---------|----------------|
| LSX  | 128 位  | `vr0`–`vr31` | 与 f0–f31 低 128 位复用 |
| LASX | 256 位  | `xr0`–`xr31` | 与 f0–f31 低 256 位复用；LASX 是 LSX 超集 |

> `vr0` 即 LSX 128 位视图，`xr0` 即 LASX 256 位视图，`f0` 即基础浮点 64 位视图——三者映射到同一物理寄存器低位，使用时不可同时混用不同视图。

#### 助记符规律

- **LSX 前缀**：`v`，如 `vadd.w`、`vld`、`vfmadd.s`
- **LASX 前缀**：`xv`，如 `xvadd.w`、`xvld`、`xvfmadd.s`
- **元素宽度后缀**：`.b`（8 位）/ `.h`（16 位）/ `.w`（32 位）/ `.d`（64 位）；浮点用 `.s`（单精度）/ `.d`（双精度）

#### 代表性指令速查

| 指令（LSX / LASX） | 说明 |
|-------------------|------|
| `vadd.w vd, vj, vk` / `xvadd.w xd, xj, xk` | 按 32 位元素并行加法（LSX 4 路，LASX 8 路） |
| `vsub.h vd, vj, vk` / `xvsub.h xd, xj, xk` | 按 16 位元素并行减法 |
| `vmul.w vd, vj, vk` | 按 32 位元素并行乘法（取低 32 位） |
| `vmadd.w vd, vj, vk` | 按 32 位元素乘加：`vd += vj * vk` |
| `vld vd, rj, si12` / `xvld xd, rj, si12` | 从内存 `rj + si12` 处加载 128/256 位到向量寄存器 |
| `vst vd, rj, si12` / `xvst xd, rj, si12` | 将向量寄存器存储到内存 `rj + si12` 处 |
| `vfmadd.s vd, vj, vk, va` | 单精度向量融合乘加：`vd = vj * vk + va`（LSX 4 路） |
| `xvfmadd.d xd, xj, xk, xa` | 双精度向量融合乘加（LASX 4 路） |
| `vshuf.b vd, vj, vk, va` | 按字节重排（类 x86 `PSHUFB`） |
| `vseq.w vd, vj, vk` | 按 32 位元素比较相等，结果填 0 或全 1 掩码 |
| `vsllwil.w.h vd, vj, ui` | 元素宽度扩展左移（`.h`→`.w`，宽度翻倍） |

#### 示例：LSX 向量加法（`int32_t` 数组，4 路并行）

```asm
# 设 $a0 = 数组 A 地址，$a1 = 数组 B 地址，$a2 = 结果数组地址
vld     $vr0, $a0, 0       # 加载 A[0..3]（4×int32，共 128 位）
vld     $vr1, $a1, 0       # 加载 B[0..3]
vadd.w  $vr2, $vr0, $vr1   # 4 路 int32 并行加法
vst     $vr2, $a2, 0       # 写回结果
```

#### 示例：LASX 单精度点积（8 路 float，需手动归约）

```asm
# 设 $a0 = float 数组 A 地址，$a1 = float 数组 B 地址
xvld    $xr0, $a0, 0       # 加载 A[0..7]（8×float32，共 256 位）
xvld    $xr1, $a1, 0       # 加载 B[0..7]
xvfmul.s $xr2, $xr0, $xr1  # 8 路 float 并行乘法
# 归约（将 xr2 的 8 个元素求和）需配合 xvhaddw 或分段提取后标量求和
# 具体归约序列见 [[21.Asm/35 SIMD字符串处理实战.md]]
```

> 更多向量扩展横向对比（LSX vs. NEON vs. AVX2 vs. RVV）见 [[21.Asm/26 向量扩展对比.md]]；SIMD 字符串处理实战示例（含归约、掩码比较等完整代码）见 [[21.Asm/35 SIMD字符串处理实战.md]]。

## 内存模型

LoongArch64 采用弱内存序模型，通过显式的内存屏障指令来控制内存访问顺序：
- `dbar`：数据内存屏障
- `ibar`：指令内存屏障

## 异常与中断

LoongArch64 定义了异常和中断处理机制，包括：
- 精确异常：确保异常发生时程序状态的一致性
- 多级中断：支持多个特权级的中断处理
- 向量中断：支持快速中断向量处理

## 与主流架构的对比

### 与 RISC-V 对比
- 指令格式相对统一，整体设计更收敛
- 特权架构定义不同（LoongArch 用 PLV0-PLV3 与一组 CSR）
- 简洁的设计哲学

### 与 ARM 对比
- 整数运算不使用条件执行和标志位寄存器（ARM 有 CPSR/NZCV）
- 指令格式更加规整
- 浮点指令遵循 IEEE 754 标准，与 ARM 类似但命名更一致

### 与 x86-64 对比
- 定长指令格式 vs x86 的变长指令
- 更规整的寻址方式
- 更简化的指令集设计
- 没有 x86 的复杂标志位系统（EFLAGS/RFLAGS）

### 与 MIPS 对比
- 相似的“无标志位、比较 + 分支”设计理念，但更现代化
- 统一的指令编码格式
- 更完善的浮点和向量指令集
- 用 CSR 体系取代了 MIPS 的协处理器概念

## 工具链支持

LoongArch 已经建立了完整的工具链生态：
- GCC 编译器支持
- LLVM 编译器支持
- GDB 调试器支持
- Binutils 工具集支持

## 总结

LoongArch64 是一个现代化的、简洁的、高性能的指令集架构，体现了龙芯团队在处理器架构设计方面的积累。LoongArch32 与 LoongArch64 共享相同的基础架构和设计哲学，但在数据宽度和地址空间上有所限制。

其核心特征可概括为：
- **从零设计、自主可控**：不兼容旧指令集包袱，编码空间规整。
- **无整数标志位**：条件判断走 `slt` 系列 + 条件分支，浮点比较走 fcc 条件标志。
- **模块化扩展**：基础整数 + 浮点 + LSX/LASX 向量 + LVZ 虚拟化。

该架构不仅具备良好的性能潜力，还拥有清晰的扩展路径，为构建自主可控的信息技术体系提供了坚实的基础。

## 相关架构

- [[21.Asm/11 ARM.md|ARM 汇编指令详解]]
- [[21.Asm/12 MIPS.md|MIPS 汇编指令详解]]
- [[21.Asm/15 x86(64).md|x86 汇编指令详解]]
- [[21.Asm/14 RISC-V.md|RISC-V 指令集详解]]
- [[21.Asm/01 汇编指令集对比.md|五架构对比总览]]
