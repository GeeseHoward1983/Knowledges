---
aliases: [MIPS汇编, MIPS指令集, MIPS32, MIPS64]
tags: [汇编/MIPS, 指令集, 计算机体系结构]
---

# MIPS 汇编指令详解：32 位与 64 位对比

## 概述

MIPS（Microprocessor without Interlocked Pipeline Stages）是一种精简指令集计算机（RISC）架构。MIPS 架构有多个版本，主要分为 MIPS32 和 MIPS64 两种，分别对应 32 位和 64 位系统。

## MIPS 寄存器

### MIPS32 寄存器
MIPS32 架构具有 32 个通用寄存器，每个寄存器 32 位宽：
- $0-$31，或用名称表示 $zero, $at, $v0, $v1, $a0-$a3, $t0-$t9, $s0-$s7 等

### MIPS64 寄存器
MIPS64 架构同样有 32 个通用寄存器，但每个寄存器 64 位宽：
- $0-$31，可访问完整的 64 位数据

### 浮点寄存器
MIPS 架构还提供 32 个浮点寄存器：
- $f0-$f31，用于浮点运算
- 在 MIPS32 中，这些寄存器通常是 32 位宽，主要用于单精度运算
- 在 MIPS64 中，这些寄存器通常是 64 位宽，支持双精度运算
- 浮点寄存器可以成对使用来处理双精度浮点数（如 $f0/$f1 组合）
- 浮点寄存器也支持向量运算，每个寄存器可存储多个较小的数据元素

#### 浮点控制状态寄存器（FCSR）
- MIPS 浮点单元还包括一个控制状态寄存器（FCSR）
- FCSR 包含浮点异常标志、舍入模式控制等
- 通过 CTC1/DMTC1 指令写入，CFC1/DMFC1 指令读取

## 指令格式类型

MIPS 指令主要有三种格式：

1. R 型（寄存器型）：用于寄存器到寄存器操作
2. I 型（立即数型）：用于立即数操作或分支操作
3. J 型（跳转型）：用于跳转操作

### R 型指令格式
```
[31-26] [25-21] [20-16] [15-11] [10-6] [5-0]
 opcode   rs      rt     rd    shamt  funct
```

### I 型指令格式
```
[31-26] [25-21] [20-16] [15-0]
 opcode   rs      rt    immediate
```

### J 型指令格式
```
[31-26] [25-0]
 opcode   target
```

## 浮点指令

MIPS 架构通过协处理器 1（Coprocessor 1）处理浮点运算。浮点指令通常以 "C." 开头表示浮点比较，以 ".s" 或 ".d" 后缀区分单精度和双精度。

### 浮点加载/存储指令

#### LWC1 - Load Word to Coprocessor 1（加载单精度到协处理器 1）
- `lwc1 $ft, offset($rs)`（从内存加载一个单精度浮点数到浮点寄存器）

#### SWC1 - Store Word from Coprocessor 1（从协处理器 1 存储单精度）
- `swc1 $ft, offset($rs)`（将单精度浮点数从浮点寄存器存储到内存）

#### LDC1 - Load Doubleword to Coprocessor 1（加载双精度到协处理器 1）
- `ldc1 $ft, offset($rs)`（从内存加载一个双精度浮点数到浮点寄存器）

#### SDC1 - Store Doubleword from Coprocessor 1（从协处理器 1 存储双精度）
- `sdc1 $ft, offset($rs)`（将双精度浮点数从浮点寄存器存储到内存）

### 浮点算术指令

#### ADD.S/ADD.D - 浮点加法
- `add.s $fd, $fs, $ft`（单精度加法：$fd = $fs + $ft）
- `add.d $fd, $fs, $ft`（双精度加法：$fd = $fs + $ft）

#### SUB.S/SUB.D - 浮点减法
- `sub.s $fd, $fs, $ft`（单精度减法：$fd = $fs - $ft）
- `sub.d $fd, $fs, $ft`（双精度减法：$fd = $fs - $ft）

#### MUL.S/MUL.D - 浮点乘法
- `mul.s $fd, $fs, $ft`（单精度乘法：$fd = $fs * $ft）
- `mul.d $fd, $fs, $ft`（双精度乘法：$fd = $fs * $ft）

#### DIV.S/DIV.D - 浮点除法
- `div.s $fd, $fs, $ft`（单精度除法：$fd = $fs / $ft）
- `div.d $fd, $fs, $ft`（双精度除法：$fd = $fs / $ft）

#### SQRT.S/SQRT.D - 浮点平方根
- `sqrt.s $fd, $fs`（单精度平方根：$fd = sqrt($fs)）
- `sqrt.d $fd, $fs`（双精度平方根：$fd = sqrt($fs)）

#### ABS.S/ABS.D - 浮点绝对值
- `abs.s $fd, $fs`（单精度绝对值：$fd = abs($fs)）
- `abs.d $fd, $fs`（双精度绝对值：$fd = abs($fs)）

#### NEG.S/NEG.D - 浮点取反
- `neg.s $fd, $fs`（单精度取反：$fd = -$fs）
- `neg.d $fd, $fs`（双精度取反：$fd = -$fs）

#### MOV.S/MOV.D - 浮点移动
- `mov.s $fd, $fs`（单精度移动：$fd = $fs）
- `mov.d $fd, $fs`（双精度移动：$fd = $fs）

### 浮点转换指令

#### CVT.S.W/CVT.D.W - 整数转浮点
- `cvt.s.w $fd, $fs`（将整数转换为单精度浮点）
- `cvt.d.w $fd, $fs`（将整数转换为双精度浮点）

#### CVT.W.S/CVT.W.D - 浮点转整数
- `cvt.w.s $fd, $fs`（将单精度浮点转换为整数）
- `cvt.w.d $fd, $fs`（将双精度浮点转换为整数）

#### CVT.S.D/CVT.D.S - 单双精度互转
- `cvt.s.d $fd, $fs`（双精度转单精度）
- `cvt.d.s $fd, $fs`（单精度转双精度）

### 浮点比较指令

#### C.EQ.S/C.EQ.D - 浮点相等比较
- `c.eq.s $fs, $ft`（单精度相等比较）
- `c.eq.d $fs, $ft`（双精度相等比较）

#### C.LT.S/C.LT.D - 浮点小于比较
- `c.lt.s $fs, $ft`（单精度小于比较）
- `c.lt.d $fs, $ft`（双精度小于比较）

#### C.LE.S/C.LE.D - 浮点小于等于比较
- `c.le.s $fs, $ft`（单精度小于等于比较）
- `c.le.d $fs, $ft`（双精度小于等于比较）

### 浮点分支指令

#### BC1F/BC1T - Branch on Coprocessor 1 False/True
- `bc1f label`（如果上次浮点比较结果为假则分支）
- `bc1t label`（如果上次浮点比较结果为真则分支）

## 与条件相关的指令

MIPS 架构的一个重要特点是它没有像 x86 或 ARM 那样的标志位寄存器（如 FLAGS 或 CPSR）。相反，MIPS 采用了一种不同的设计哲学：通过专门的比较指令生成布尔结果，然后使用条件分支指令进行控制流转移。这种设计符合 RISC 架构的简洁性原则。

### 比较指令

#### SLT - Set on Less Than（设置小于）
- MIPS32: `slt $rd, $rs, $rt`（如果 $rs < $rt，则 $rd = 1，否则 $rd = 0）
- MIPS64: 同名指令 `slt`，直接对 64 位寄存器进行比较（无 d 前缀变体）
- 注意：执行有符号比较

#### SLTI - Set on Less Than Immediate（设置小于立即数）
- MIPS32: `slti $rt, $rs, immediate`（如果 $rs < immediate，则 $rt = 1，否则 $rt = 0）
- MIPS64: 同名指令 `slti`，对 64 位寄存器比较

#### SLTU/SLTIU - Set on Less Than Unsigned（无符号小于）
- MIPS32: `sltu $rd, $rs, $rt` 和 `sltiu $rt, $rs, immediate`
- MIPS64: 同名指令 `sltu`/`sltiu`，对 64 位寄存器比较
- 注意：这些指令执行无符号整数比较

> 提示：MIPS 的 `slt`/`slti`/`sltu`/`sltiu` 不存在 `dslt`/`dsltu` 等 64 位专用变体——它们本身就对完整宽度的寄存器进行比较。

### 条件分支指令

#### BEQ/BNE - 相等/不等分支
- MIPS32: `beq $rs, $rt, offset` / `bne $rs, $rt, offset`
- MIPS64: 同上
- 功能：分别在 $rs 等于或不等于 $rt 时跳转

#### BGTZ/BLEZ - 大于零/小于等于零分支
- MIPS32: `bgtz $rs, offset`（如果 $rs > 0 则跳转）和 `blez $rs, offset`（如果 $rs <= 0 则跳转）
- MIPS64: 同上

#### BLTZ/BGEZ - 小于零/大于等于零分支
- MIPS32: `bltz $rs, offset`（如果 $rs < 0 则跳转）和 `bgez $rs, offset`（如果 $rs >= 0 则跳转）
- MIPS64: 同上

### 条件跳转并链接指令

这些指令不仅根据条件跳转，还会将返回地址保存到 $ra 寄存器中，用于实现条件函数调用。

#### BLTZAL/BGEZAL - Branch on Less/Greater or Equal to Zero And Link
- MIPS32: `bltzal $rs, offset` 和 `bgezal $rs, offset`
- MIPS64: 同上
- 功能：根据条件跳转到目标地址，并将返回地址存入 $ra

## 算术指令

#### ADD - 加法（R 型）
- MIPS32: `add $rd, $rs, $rt`（将 $rs 和 $rt 相加，结果存入 $rd）
- MIPS64: 同上，但处理 64 位数据
- 注意：ADD 在发生溢出时会触发异常

#### ADDU - 无符号加法（R 型）
- MIPS32: `addu $rd, $rs, $rt`（无符号加法，不检查溢出）
- MIPS64: `daddu $rd, $rs, $rt`（64 位版本使用不同指令）

#### SUB - 减法（R 型）
- MIPS32: `sub $rd, $rs, $rt`
- MIPS64: 同上，但处理 64 位数据

#### SUBU - 无符号减法（R 型）
- MIPS32: `subu $rd, $rs, $rt`
- MIPS64: `dsubu $rd, $rs, $rt`（64 位版本使用不同指令）

#### MULT/MULTU - 乘法（R 型）
- MIPS32: `mult $rs, $rt`（32 位乘以 32 位，结果放在 HI 和 LO 寄存器）
- MIPS64: `dmult $rs, $rt`（64 位乘以 64 位）

#### DIV/DIVU - 除法（R 型）
- MIPS32: `div $rs, $rt`（商放在 LO 寄存器，余数在 HI 寄存器）
- MIPS64: `ddiv $rs, $rt`（64 位版本）

### 逻辑指令

#### AND - 按位与（R 型）
- MIPS32: `and $rd, $rs, $rt`
- MIPS64: 同上，但对 64 位数据操作

#### OR - 按位或（R 型）
- MIPS32: `or $rd, $rs, $rt`
- MIPS64: 同上

#### XOR - 按位异或（R 型）
- MIPS32: `xor $rd, $rs, $rt`
- MIPS64: 同上

#### NOR - 按位或非（R 型）
- MIPS32: `nor $rd, $rs, $rt`
- MIPS64: 同上

#### SLL - 逻辑左移（R 型）
- MIPS32: `sll $rd, $rt, shamt`（左移，填 0）
- MIPS64: `dsll $rd, $rt, shamt`（64 位版本）

#### SRL - 逻辑右移（R 型）
- MIPS32: `srl $rd, $rt, shamt`（逻辑右移，填 0）
- MIPS64: `dsrl $rd, $rt, shamt`（64 位版本）

#### SRA - 算术右移（R 型）
- MIPS32: `sra $rd, $rt, shamt`（算术右移，符号扩展）
- MIPS64: `dsra $rd, $rt, shamt`（64 位版本）

### 立即数指令（I 型）

#### ADDI - 加立即数
- MIPS32: `addi $rt, $rs, immediate`（将 $rs 与立即数相加）
- MIPS64: `daddi $rt, $rs, immediate`（64 位版本）

#### ADDIU - 无符号加立即数
- MIPS32: `addiu $rt, $rs, immediate`
- MIPS64: `daddiu $rt, $rs, immediate`（64 位版本）

#### ANDI - 与立即数
- MIPS32: `andi $rt, $rs, immediate`
- MIPS64: 同上

#### ORI - 或立即数
- MIPS32: `ori $rt, $rs, immediate`
- MIPS64: 同上

#### XORI - 异或立即数
- MIPS32: `xori $rt, $rs, immediate`
- MIPS64: 同上

### 跳转指令

#### J - 无条件跳转（J 型）
- MIPS32: `j target`
- MIPS64: 同上
- 注意：直接跳转到指定地址

#### JR - 寄存器跳转（R 型）
- MIPS32: `jr $rs`
- MIPS64: 同上
- 用途：常用于函数返回（jr $ra）

#### JAL - Jump and Link
- MIPS32: `jal target`
- MIPS64: 同上
- 功能：跳转到目标地址，并将返回地址保存到 $ra 寄存器

### Load/Store 指令

#### LW/LD - 加载字/双字（I 型）
- MIPS32: `lw $rt, offset($rs)`（加载 32 位字）
- MIPS64: `ld $rt, offset($rs)`（加载 64 位双字）

#### SW/SD - 存储字/双字（I 型）
- MIPS32: `sw $rt, offset($rs)`
- MIPS64: `sd $rt, offset($rs)`

#### LH/LHU/LB/LBU - 加载半字/字节（I 型）
- MIPS32: `lh $rt, offset($rs)` / `lhu $rt, offset($rs)` / `lb $rt, offset($rs)` / `lbu $rt, offset($rs)`
- MIPS64: 同上
- 注意：`lh`/`lb` 为**符号扩展**，`lhu`/`lbu` 为**零扩展**（在 64 位下分别扩展到 64 位）

#### SH/SB - 存储半字/字节（I 型）
- MIPS32: `sh $rt, offset($rs)` / `sb $rt, offset($rs)`
- MIPS64: 同上

## 32 位与 64 位 MIPS 的主要差异

| 特性 | MIPS32 | MIPS64 |
|------|--------|--------|
| 通用寄存器宽度 | 32 位 | 64 位 |
| 浮点寄存器宽度 | 通常 32 位单精度，可配对做 64 位双精度 | 通常 64 位，支持原生双精度运算 |
| 地址空间 | 4GB | 可达 2^64 字节 |
| 数据处理能力 | 32 位数据 | 64 位数据 |
| 特殊指令 | mult, div 等 | dmult, ddiv 等 |
| Load/Store 指令 | lw, sw | ld, sd |
| 移位指令 | sll, srl | dsll, dsrl |
| 立即数指令 | addi | daddi |

## 标志位系统特点总结

与传统的 CISC 架构不同，MIPS 架构的设计避免了显式的标志位寄存器。这种设计带来了以下优势：

1. **简化流水线设计**：没有全局状态寄存器减少了数据依赖和冒险
2. **提高并行性**：指令之间较少隐式依赖，有利于指令级并行
3. **明确的数据流**：所有条件判断都通过显式的比较指令完成

典型的条件执行模式：
```mipsasm
# 判断 $t0 是否小于 $t1
slt $t2, $t0, $t1    # 如果 $t0 < $t1，则 $t2 = 1
beq $t2, $zero, else # 如果 $t2 == 0 (即不小于)，跳转到else
# then 分支代码
j end
else:
# else 分支代码
end:
```

## 应用示例

### 32 位示例
```mipsasm
add $t0, $t1, $t2    # 将$t1和$t2相加，结果存入$t0
lw $t0, 0($t1)       # 从地址$t1加载一个32位字到$t0
```

### 64 位示例
```mipsasm
dadd $t0, $t1, $t2   # 将$t1和$t2进行64位相加
ld $t0, 0($t1)       # 从地址$t1加载一个64位双字到$t0
```

## 特殊功能

MIPS64 相比 MIPS32 增加了以下特性：
1. 支持更大的虚拟地址空间
2. 提供对 64 位数据的原子操作
3. 扩展了寻址模式
4. 更多的寄存器（可选的 MIPS64R2 规范）

## HI/LO 寄存器与 MFHI/MFLO

### 为何独立于通用寄存器

MIPS 32×32 乘法产生 64 位结果，32×64 乘法产生 128 位结果，无法放入单个通用寄存器。设计者将乘除结果拆存于两个专用寄存器 HI（高半部）和 LO（低半部），与通用寄存器文件物理隔离，避免占用通用寄存器资源，也使流水线对乘除结果的旁路/写回更简单。

### 乘除法与 HI/LO 读写指令

```mipsasm
# 有符号乘法：$rs × $rt → HI:LO（64 位结果）
mult  $t0, $t1          # HI = 高32位，LO = 低32位

# 无符号乘法
multu $t0, $t1

# 有符号除法：$rs ÷ $rt → LO = 商，HI = 余数
div   $t0, $t1          # 注意：不检查除以零，需软件保护

# 无符号除法
divu  $t0, $t1

# 从 HI/LO 读取结果（有 2 个流水线周期延迟要求，需避免紧跟依赖）
mfhi  $v0               # $v0 = HI
mflo  $v1               # $v1 = LO

# 向 HI/LO 写入（用于恢复上下文或特殊初始化）
mthi  $t2               # HI = $t2
mtlo  $t3               # LO = $t3
```

### 64 位版本

MIPS64 提供 `dmult`/`dmultu`/`ddiv`/`ddivu`，操作数与结果均为 64 位，HI/LO 同样存 128/64 位结果。读写接口不变（`mfhi`/`mflo`/`mthi`/`mtlo`）。

### 注意事项

- 两次连续的乘除指令之间，若后一条在前一条完成前就读 HI/LO，结果未定义（需 2 条无关指令间隔，或等流水线排空）。
- MIPS R6 移除了 `MULT`/`DIV` + HI/LO 机制，改用直接写目标寄存器的新指令（详见下文 MIPS R6 变化小节）。

---

## CP0 协处理器（系统控制协处理器）

### 定位与访问方式

CP0（Coprocessor 0）是 MIPS 架构内置的特权控制单元，负责异常/中断处理、MMU 管理、缓存控制和处理器模式切换。用户态程序无法访问 CP0（触发 Reserved Instruction 或 Coprocessor Unusable 异常）。

```mipsasm
# 读 CP0 寄存器：MFC0（Move From Coprocessor 0）
mfc0  $t0, $12          # $t0 = CP0 寄存器 12（Status）
mfc0  $t1, $13          # $t1 = CP0 寄存器 13（Cause）

# 写 CP0 寄存器：MTC0（Move To Coprocessor 0）
mtc0  $t2, $12          # CP0 Status = $t2

# MIPS64 双字访问（操作 64 位 CP0 寄存器）
dmfc0 $t0, $5           # PageMask 等 64 位字段
dmtc0 $t0, $5
```

### 关键寄存器速查

| 寄存器编号 | 名称 | 主要用途 |
|-----------|------|---------|
| $8 | BadVAddr | 记录最近一次地址异常的错误虚拟地址 |
| $9 | Count | 自由运行计数器，与 Compare 配合产生定时器中断 |
| $11 | Compare | 与 Count 匹配时触发中断（写入同时清除中断挂起） |
| $12 | Status | 全局中断使能（IE）、用户/内核模式（UM）、异常级（EXL）、错误级（ERL）等 |
| $13 | Cause | 异常原因码（ExcCode）、挂起中断位（IP）、分支延迟槽标志（BD） |
| $14 | EPC | 异常返回地址（Exception Program Counter），`eret` 跳回此地址 |
| $10 | EntryHi | TLB 条目高半部：虚拟页号（VPN2）+ ASID |
| $2/$3 | EntryLo0/1 | TLB 条目低半部：物理帧号（PFN）、有效位（V）、脏位（D）等 |
| $0 | Index | TLB 操作索引 |
| $4 | Context | 用于快速异常处理的页表基址 + 查询偏移 |

### 异常/中断处理流程

```mipsasm
# 内核异常向量（简化示意）
exception_handler:
    mfc0  $k0, $13        # 读 Cause，获取 ExcCode
    mfc0  $k1, $14        # 读 EPC，保存返回地址
    andi  $k0, $k0, 0x7c  # 提取 ExcCode（bits[6:2]）
    beq   $k0, $zero, int_handler  # ExcCode=0 → 外部中断
    # ... 分发到各异常处理函数 ...

    # 处理完毕后恢复并返回
    mtc0  $k1, $14        # 恢复 EPC（若修改过）
    eret                   # 异常返回：清 EXL，跳回 EPC
```

### MMU 操作（TLB 管理）

```mipsasm
# TLB 写操作
mtc0  $t0, $10           # EntryHi = VPN2 + ASID
mtc0  $t1, $2            # EntryLo0 = 偶页 PFN/V/D/C
mtc0  $t2, $3            # EntryLo1 = 奇页 PFN/V/D/C
tlbwi                    # 将上述值写入 Index 指定的 TLB 槽
# 或 tlbwr               # 写入随机槽（Random 寄存器）

# TLB 读操作
tlbr                     # 将 Index 槽内容读入 EntryHi/Lo0/Lo1

# TLB 查找
mtc0  $t0, $10           # EntryHi = 查询 VPN2 + ASID
tlbp                     # 探测匹配项，结果写入 Index（未命中则 Index[31]=1）
mfc0  $t3, $0            # 读取 Index
```

---

## MIPS R6 变化（MIPS32r6 / MIPS64r6）

### 背景

2014 年发布的 MIPS Release 6（R6）是对经典 MIPS 架构最大的一次"破坏性重构"，移除了若干历史包袱、重新编码了部分指令空间，并引入紧凑型分支指令，与 R2/R5 代码**二进制不兼容**。

### 移除 MULT/DIV + HI/LO，改用直接写通用寄存器

R6 彻底移除了 `MULT`/`MULTU`/`DIV`/`DIVU`/`MFHI`/`MFLO`/`MTHI`/`MTLO` 及 HI/LO 寄存器。替代指令直接将结果写入指定通用寄存器：

```mipsasm
# R2/R5 写法（HI:LO）
mult  $t0, $t1
mflo  $v0               # 取低32位（乘积低半）
mfhi  $v1               # 取高32位

# R6 写法（直接写目标寄存器）
mul   $v0, $t0, $t1     # $v0 = ($t0 × $t1) 的低 32 位
muh   $v1, $t0, $t1     # $v1 = ($t0 × $t1) 的高 32 位（有符号）
mulu  $v0, $t0, $t1     # 无符号乘低位
muhu  $v1, $t0, $t1     # 无符号乘高位

div   $v0, $t0, $t1     # $v0 = 商（有符号）
mod   $v1, $t0, $t1     # $v1 = 余数（有符号）
divu  $v0, $t0, $t1     # 无符号除，商
modu  $v1, $t0, $t1     # 无符号除，余数
```

### 分支延迟槽行为变化

经典 MIPS（R1-R5）所有分支/跳转指令均有一个**分支延迟槽**：跳转指令执行时，其后紧跟的一条指令总会被执行，之后才真正跳转。R6 引入**紧凑型（Compact）分支指令**，无延迟槽：

```mipsasm
# R2/R5 分支（有延迟槽）
beq   $t0, $t1, target  # 延迟槽指令总被执行
  nop                   # 常见填充（或有用的指令）

# R6 紧凑型分支（无延迟槽）
beqc  $t0, $t1, target  # 立即跳转，无延迟槽
bc    target             # R6 无条件紧凑跳转
```

> R6 并未完全移除带延迟槽的 `BEQ`/`BNE` 等，但新增了一批 `*C` 后缀紧凑变体以替代；部分传统指令被重新编码为 R6 专有语义。

### 指令重编码与移除

| 指令 | R2/R5 状态 | R6 变化 |
|------|-----------|---------|
| `MULT`/`MULTU` | 写 HI:LO | **移除**，用 `MUL`/`MUH` 替代 |
| `DIV`/`DIVU` | 写 HI:LO | **移除**，用 `DIV`/`MOD` 替代 |
| `MFHI`/`MFLO` | 读 HI/LO | **移除** |
| `MADD`/`MSUB` | 乘加到 HI:LO | **移除** |
| `LWL`/`LWR` | 非对齐加载 | **移除**（改用 `ALIGN` 指令） |
| `LL`/`SC` | 独占加载/存储 | **保留但调整**：R6 的 `LL`/`SC` 取消了"地址必须与 R2 相同才成功"的隐式约束，改为更显式的失败条件 |
| `JALR` | 跳转并链接 | 保留；`JALR.HB` 用于清 I-Cache 执行障碍 |

### 区分 R2/R5 与 R6 代码的方法

1. **反汇编器提示**：`objdump` 若遇到 R6 编码的旧指令位置会报 `<unknown>`，反之亦然；`-M` 选项可指定 ISA 版本。
2. **ELF e_flags 字段**：`readelf -h` 查看 `e_flags`；R6 对应 `EF_MIPS_ARCH_32R6`（0x90000000）或 `EF_MIPS_ARCH_64R6`（0xa0000000）。
3. **关键指令特征**：有 `mfhi`/`mflo` → R2/R5；有 `muh`/`mod` → R6；有 `beqc`/`bnec` 等紧凑分支 → R6。
4. **工具链标志**：GCC `-march=mips32r6` / `-march=mips64r6` 生成 R6 代码；默认（`mips32r2`）生成 R2 兼容代码。

---

## 总结

MIPS 以“规整、正交、易于流水线化”著称，是经典 RISC 教学的范本：

- **三种定长指令格式**（R/I/J）使译码极其简单。
- **无标志位设计**，条件判断全部交给显式的 `slt` 系列加条件分支，消除了隐式状态依赖。
- **32 位到 64 位**主要通过新增 `d` 前缀指令（dadd、dmult、dsll 等）扩展，而比较类指令保持原名。

凭借简洁性和规则性，MIPS 在嵌入式系统、网络设备和教学领域得到了广泛应用。

## 相关架构

- [[21.Asm/11 ARM.md|ARM 汇编指令详解]]
- [[21.Asm/13 LoongArch64.md|LoongArch64 指令集详解]]
- [[21.Asm/15 x86(64).md|x86 汇编指令详解]]
- [[21.Asm/14 RISC-V.md|RISC-V 指令集详解]]
- [[21.Asm/01 汇编指令集对比.md|五架构对比总览]]
