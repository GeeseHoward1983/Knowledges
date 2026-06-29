---
aliases: [ARM汇编, ARM指令集, AArch32, AArch64]
tags: [汇编/ARM, 指令集, 计算机体系结构]
---

# ARM 汇编指令详解：32 位与 64 位对比

## ARM 架构概述

ARM（Advanced RISC Machine）是一种精简指令集（RISC）处理器架构。在 ARM 架构的发展过程中，主要有两个重要版本系列：
- ARMv7-A：支持 32 位指令集（AArch32）
- ARMv8-A：引入 64 位指令集（AArch64）

## 寄存器详解

### 32 位架构寄存器（AArch32）
ARM 32 位处理器有 16 个通用寄存器（R0-R15）：
- R0-R12：通用寄存器
- R13（SP）：栈指针寄存器
- R14（LR）：链接寄存器
- R15（PC）：程序计数器

状态寄存器：
- CPSR（Current Program Status Register）：当前程序状态寄存器
- SPSR（Saved Program Status Register）：保存的程序状态寄存器

### 64 位架构寄存器（AArch64）
ARM 64 位处理器有 32 个通用寄存器（W0-W30, X0-X30）：
- W0-W30：32 位寄存器
- X0-X30：64 位寄存器（W 寄存器是其低 32 位）
- SP：栈指针
- PC：程序计数器（不能直接访问）
- LR（X30）：链接寄存器
- NZCV：条件标志寄存器
- DAIF：中断屏蔽位

## 标志位详解（CPSR/NZCV）

### 32 位架构中的 CPSR 寄存器

在 ARM 32 位架构中，当前程序状态寄存器（CPSR - Current Program Status Register）包含重要的标志位：

- **N (Negative) 负数标志 - bit 31**：当操作结果为负数时置 1，否则清 0
- **Z (Zero) 零标志 - bit 30**：当操作结果为零时置 1，否则清 0
- **C (Carry) 进位标志 - bit 29**：对于加法操作，当发生进位时置 1；对于减法操作，当无借位时置 1
- **V (Overflow) 溢出标志 - bit 28**：当有符号整数运算结果超出表示范围时置 1
- **Q (Saturation) 饱和标志 - bit 27**：饱和发生时置 1
- **IT[1:0] (IT Block) - bit 26-25**：IT 块标志位
- **J (Jazelle) - bit 24**：Jazelle 位
- **E (Endianness) - bit 9**：大端/小端执行状态位
- **A (Async abort) - bit 8**：禁用异步中止位
- **I (IRQ interrupt) - bit 7**：禁用 IRQ 中断位
- **F (FIQ interrupt) - bit 6**：禁用 FIQ 中断位
- **T (Thumb state) - bit 5**：Thumb 状态位
- **M[4:0] (Mode) - bit 4-0**：处理器模式位

### 64 位架构中的 NZCV 寄存器

在 ARM 64 位架构中，条件标志被分离出来，存储在 NZCV 特殊寄存器中：
- **bit 31 (N)**：负数标志 (Negative flag)
- **bit 30 (Z)**：零标志 (Zero flag)
- **bit 29 (C)**：进位标志 (Carry flag)
- **bit 28 (V)**：溢出标志 (oVerflow flag)

## 基本数据处理指令详解

### 数据传送指令

#### 32 位架构
- `MOV Rd, Rn`：将寄存器 Rn 的值传送到寄存器 Rd
- `MOVT Rd, #imm16`：将 16 位立即数移到目标寄存器的高 16 位
- `MOVW Rd, #imm16`：将 16 位立即数移到目标寄存器的低 16 位
- `MVN Rd, Rn`：取反传送（NOT 操作）
- `LDR Rd, [Rn]`：从内存加载数据到寄存器
- `LDR Rd, =immediate`：加载立即数到寄存器
- `PUSH {Rn}`：将寄存器压入栈
- `POP {Rn}`：从栈弹出寄存器

#### 64 位架构
- `MOV Xd, Xs`：数据传送
- `MOVZ Xd, #imm16`：将 16 位立即数移到目标寄存器指定的 16 位段（其余位清零）
- `MOVK Xd, #imm16, LSL #shift`：将 16 位立即数移到目标寄存器指定的 16 位段，不改变其他位
- `ADR Xd, label`：计算标签地址
- `ADRP Xd, label`：计算页面地址
- `LDR Xt, [Xn]`：加载寄存器
- `LDR Xt, =immediate`：加载立即数
- `STR Xt, [Xn]`：存储寄存器

### 算术运算指令

#### 32 位架构
- `ADD Rd, Rn, Rm`：加法运算
- `SUB Rd, Rn, Rm`：减法运算
- `RSB Rd, Rn, Rm`：逆向减法（Rm - Rn）
- `ADC Rd, Rn, Rm`：带进位加法
- `SBC Rd, Rn, Rm`：带借位减法
- `RSC Rd, Rn, Rm`：带借位逆向减法
- `MUL Rd, Rn, Rm`：乘法运算
- `MLA Rd, Rn, Rm, Ra`：乘加运算
- `MLS Rd, Rn, Rm, Ra`：乘减运算
- `SDIV Rd, Rn, Rm`：有符号除法
- `UDIV Rd, Rn, Rm`：无符号除法

#### 64 位架构
- `ADD Xd, Xn, Xm`：加法
- `SUB Xd, Xn, Xm`：减法
- `MUL Xd, Xn, Xm`：乘法
- `SDIV Xd, Xn, Xm`：有符号除法
- `UDIV Xd, Xn, Xm`：无符号除法
- `SMULL Xd, Wn, Wm`：32 位有符号数相乘，结果为 64 位
- `UMULL Xd, Wn, Wm`：32 位无符号数相乘，结果为 64 位

### 逻辑运算指令

#### 32 位架构
- `AND Rd, Rn, Rm`：按位与
- `ORR Rd, Rn, Rm`：按位或
- `EOR Rd, Rn, Rm`：按位异或
- `BIC Rd, Rn, Rm`：按位清除（AND NOT）
- `ORN Rd, Rn, Rm`：按位或非（OR NOT）

#### 64 位架构
- `AND Xd, Xn, Xm`：按位与
- `ORR Xd, Xn, Xm`：按位或
- `EOR Xd, Xn, Xm`：按位异或
- `BIC Xd, Xn, Xm`：按位清除
- `ORN Xd, Xn, Xm`：按位或非（OR NOT）
- `EON Xd, Xn, Xm`：按位异或非（EOR NOT）

### 移位操作指令

#### 32 位架构
- `LSL Rd, Rm, #n`：逻辑左移
- `LSR Rd, Rm, #n`：逻辑右移
- `ASR Rd, Rm, #n`：算术右移
- `ROR Rd, Rm, #n`：循环右移
- `RRX Rd, Rm`：带扩展的循环右移

#### 64 位架构
- `LSL Xd, Xn, #shift`：逻辑左移
- `LSR Xd, Xn, #shift`：逻辑右移
- `ASR Xd, Xn, #shift`：算术右移
- `ROR Xd, Xn, #shift`：循环右移

### 比较指令

#### 32 位架构
- `CMP Rn, Rm`：比较两个操作数
- `CMN Rn, Rm`：负数比较
- `TST Rn, Rm`：测试操作
- `TEQ Rn, Rm`：相等测试

#### 64 位架构
- `CMP Xn, Xm`：比较两个操作数
- `CMN Xn, Xm`：负数比较
- `TST Xn, Xm`：测试操作（按位与并更新标志）

> 注：AArch64 取消了 AArch32 的 `TEQ` 指令，相等判断改用 `CMP` 后接条件分支。

### 位操作指令

#### 32 位架构
- `CLZ Rd, Rm`：计算前导零的个数
- `REV Rd, Rm`：字节反转
- `REV16 Rd, Rm`：半字内字节反转
- `RBIT Rd, Rm`：位反转

#### 64 位架构
- `CLZ Xd, Xm`：计算前导零的个数
- `RBIT Xd, Xm`：位反转
- `REV Xd, Xm`：字节反转
- `REV16 Xd, Xm`：半字内字节反转
- `REV32 Xd, Xm`：32 位字内字节反转

## 加载/存储指令详解

### 32 位架构加载/存储指令
- `LDR Rt, [Rn, #offset]`：基本加载指令
- `STR Rt, [Rn, #offset]`：基本存储指令
- `LDM/LDMIA/LDMIB`：加载多个寄存器
- `STM/STMIA/STMIB`：存储多个寄存器
- `LDRB Rt, [Rn, #offset]`：加载字节（零扩展）
- `LDRH Rt, [Rn, #offset]`：加载半字（零扩展）
- `LDRSB Rt, [Rn, #offset]`：加载有符号字节（符号扩展到 32 位）
- `LDRSH Rt, [Rn, #offset]`：加载有符号半字（符号扩展到 32 位）
- `STRB Rt, [Rn, #offset]`：存储字节
- `STRH Rt, [Rn, #offset]`：存储半字

### 64 位架构加载/存储指令
- `LDR Xt, [Xn, #offset]`：加载 64 位
- `STR Xt, [Xn, #offset]`：存储 64 位
- `LDR Wt, [Xn, #offset]`：加载 32 位（写入 W 寄存器时自动零扩展到 64 位）
- `LDRSW Xt, [Xn, #offset]`：加载 32 位（符号扩展到 64 位）
- `LDP Xt1, Xt2, [Xn, #offset]`：加载配对寄存器
- `STP Xt1, Xt2, [Xn, #offset]`：存储配对寄存器
- `LDRB Wt, [Xn, #offset]`：加载字节（零扩展）
- `LDRH Wt, [Xn, #offset]`：加载半字（零扩展）
- `LDRSB Xt, [Xn, #offset]`：加载有符号字节（符号扩展到 64 位）
- `LDRSH Xt, [Xn, #offset]`：加载有符号半字（符号扩展到 64 位）
- `STRB Wt, [Xn, #offset]`：存储字节
- `STRH Wt, [Xn, #offset]`：存储半字

## 与标志位相关的指令

### 影响标志位的数据处理指令

#### 32 位架构
在 32 位 ARM 中，大多数数据处理指令可以通过添加 'S' 后缀来更新标志位：
- `ADDS R0, R1, R2`：加法并更新标志
- `SUBS R0, R1, R2`：减法并更新标志
- `RSBS R0, R1, #100`：逆向减法并更新标志
- `MOVS R0, R1`：传送并更新标志
- `MVNS R0, R1`：取反并更新标志
- `ANDS R0, R1, R2`：与操作并更新标志
- `ORRS R0, R1, R2`：或操作并更新标志
- `EORS R0, R1, R2`：异或并更新标志
- `MULS R0, R1, R2`：乘法并更新标志

#### 64 位架构
在 64 位 ARM 中，更新标志位的指令收敛为少数几条带 'S' 后缀的变体（注意 AArch64 没有 `MOVS`/`ORRS`/`MULS`）：
- `ADDS X0, X1, X2`：加法并更新标志
- `SUBS X0, X1, X2`：减法并更新标志
- `ADCS X0, X1, X2`：带进位加法并更新标志
- `SBCS X0, X1, X2`：带借位减法并更新标志
- `ANDS X0, X1, X2`：与操作并更新标志
- `BICS X0, X1, X2`：按位清除并更新标志

### 专门的比较指令

#### 32 位架构
- `CMP Rn, Rm`：计算 Rn - Rm 并更新 NZCV 标志
- `CMN Rn, Rm`：计算 Rn + Rm 并更新 NZCV 标志
- `TST Rn, Rm`：计算 Rn AND Rm 并更新 NZCV 标志
- `TEQ Rn, Rm`：计算 Rn EOR Rm 并更新 NZCV 标志

#### 64 位架构
- `CMP Xn, Xm`：计算 Xn - Xm 并更新 NZCV 标志
- `CMN Xn, Xm`：计算 Xn + Xm 并更新 NZCV 标志
- `TST Xn, Xm`：计算 Xn AND Xm 并更新 NZCV 标志

### 条件执行指令

ARM 架构的一个强大特性是条件执行，指令可以根据标志位的状态有条件地执行：

#### 32 位架构条件后缀
- `EQ`：等于（Z=1）
- `NE`：不等于（Z=0）
- `CS/HS`：无符号更高或相同（C=1）
- `CC/LO`：无符号更低（C=0）
- `MI`：负数（N=1）
- `PL`：正数或零（N=0）
- `VS`：溢出（V=1）
- `VC`：无溢出（V=0）
- `HI`：无符号更高（C=1 且 Z=0）
- `LS`：无符号更低或相等（C=0 或 Z=1）
- `GE`：有符号大于或等于（N=V）
- `LT`：有符号小于（N≠V）
- `GT`：有符号大于（Z=0 且 N=V）
- `LE`：有符号小于或等于（Z=1 或 N≠V）
- `AL`：总是（隐含，默认情况）

示例：
- `ADDEQ R0, R1, R2`：仅当 Z=1 时执行加法
- `SUBNE R0, R1, R2`：仅当 Z=0 时执行减法
- `MOVEQ R0, #0`：仅当 Z=1 时传送 0 到 R0

#### 64 位架构条件指令
AArch64 移除了大部分指令后缀式条件执行，改用专门的条件选择/比较指令：
- `CSET Wd, cond`：条件设置（如果条件为真则设为 1，否则设为 0）
- `CINC Wd, Wn, cond`：条件增加（如果条件为真则增加 1）
- `CSETM Wd, cond`：条件设置掩码（如果条件为真则设为全 1，否则设为 0）
- `CSINC Wd, Wn, Wm, cond`：条件选择递增
- `CSINV Wd, Wn, Wm, cond`：条件选择取反
- `CCMN Wn, Wm, #nzcv, cond`：条件比较（加法形式）
- `CCMP Wn, Wm, #nzcv, cond`：条件比较（减法形式）

### 直接操作标志位的指令

#### 32 位架构
- `MRS Rd, CPSR`：将 CPSR 复制到通用寄存器
- `MSR CPSR_flg, Rm`：将通用寄存器的值写入 CPSR 的标志位
- `MSR CPSR_c, #immediate`：将立即数写入 CPSR 的控制位

#### 64 位架构
- `MRS Xd, NZCV`：将 NZCV 标志寄存器复制到 Xd
- `MSR NZCV, Xn`：将 Xn 的值写入 NZCV 标志寄存器
- `MRS Xd, DAIF`：读取中断屏蔽位
- `MSR DAIF, Xn`：设置中断屏蔽位

## 分支跳转指令

### 32 位架构
- `B label`：无条件跳转
- `BL label`：带链接的跳转（调用子程序）
- `BX Rn`：分支并交换（切换 Thumb/ARM 状态）
- `BLX Rn`：带链接和交换的分支
- 条件分支如 `BEQ`, `BNE`, `BCS`, `BCC`, `BMI`, `BPL`, `BVS`, `BVC`, `BHI`, `BLS`, `BGE`, `BLT`, `BGT`, `BLE`

### 64 位架构
- `B label`：无条件分支
- `BL label`：带链接的分支（函数调用）
- `BR Xn`：寄存器分支
- `BLR Xn`：带链接的寄存器分支
- `RET {Xn}`：返回指令
- 条件分支：`B.cond` 如 `B.EQ`, `B.NE`, `B.CS`, `B.CC`, `B.MI`, `B.PL`, `B.VS`, `B.VC`, `B.HI`, `B.LS`, `B.GE`, `B.LT`, `B.GT`, `B.LE`

## 特殊数据处理指令

### 信号处理指令（SIMD）
#### 32 位架构
- `SMLABB Rd, Rn, Rm, Ra`：有符号乘法，最底字节相乘
- `SMLABT Rd, Rn, Rm, Ra`：有符号乘法，底字节和顶字节相乘
- `SMLATB Rd, Rn, Rm, Ra`：有符号乘法，顶字节和底字节相乘
- `SMLATT Rd, Rn, Rm, Ra`：有符号乘法，顶字节相乘
- `UMLAL Rd_lo, Rd_hi, Rn, Rm`：无符号长乘累加

#### 64 位架构
- `SMADDL Xd, Wn, Wm, Xa`：有符号 32×32→64 位乘加
- `UMADDL Xd, Wn, Wm, Xa`：无符号 32×32→64 位乘加
- `SMSUBL Xd, Wn, Wm, Xa`：有符号 32×32→64 位乘减
- `UMSUBL Xd, Wn, Wm, Xa`：无符号 32×32→64 位乘减

### 原子操作指令
- `LDREX Rt, [Rn]`：独占加载
- `STREX Rd, Rt, [Rn]`：独占存储
- `CLREX`：清除独占监控

## 标志位的使用实例

### 32 位示例
```armasm
    MOVW R0, #0x1234      @ 将0x1234放入R0的低16位
    MOVT R0, #0x5678      @ 将0x5678放入R0的高16位，R0 = 0x56781234
    LDRSB R1, [R2]        @ 从R2指向的地址加载一个有符号字节到R1（符号扩展）
    LDRSH R3, [R4]        @ 从R4指向的地址加载一个有符号半字到R3（符号扩展）
    CMP R1, R2            @ 比较R1和R2，更新标志
    ORN R3, R4, R5        @ R3 = R4 OR NOT(R5)
    MOVEQ R0, #1          @ 如果R1==R2，则R0=1
    MOVNE R0, #0          @ 如果R1!=R2，则R0=0
    ADDVS R3, R4, R5      @ 如果溢出则执行加法
    SUBCS R6, R7, R8      @ 如果无符号更高或相等则执行减法
```

### 64 位示例
```armasm
    MOVZ X0, #0x1234              @ 将0x1234放入X0的0-15位
    MOVK X0, #0x5678, LSL #16    @ 将0x5678放入X0的16-31位
    MOVK X0, #0x9ABC, LSL #32    @ 将0x9ABC放入X0的32-47位
    LDRSB X1, [X2]               @ 从X2指向的地址加载一个有符号字节到X1（符号扩展）
    LDRSH X3, [X4]               @ 从X4指向的地址加载一个有符号半字到X3（符号扩展）
    LDRSW X5, [X6]               @ 从X6指向的地址加载一个有符号32位到X5（符号扩展到64位）
    CMP X1, X2                   @ 比较X1和X2，更新标志
    ORN X3, X4, X5               @ X3 = X4 OR NOT(X5)
    CSET X0, EQ                  @ 如果X1==X2，则X0=1，否则X0=0
    CINC X5, X5, LT              @ 如果X1<X2，则X5++
    CSINC X3, X4, X5, GT         @ 如果X1>X2，则X3=X4+1，否则X3=X5+1
```

## 标志位在分支中的应用

### 32 位架构
- `BEQ label`：等于时分支
- `BNE label`：不等于时分支
- `BCS/BHS label`：无符号更高或相同时分支
- `BCC/BLO label`：无符号更低时分支
- `BMI label`：负数时分支
- `BPL label`：正数或零时分支
- `BVS label`：溢出时分支
- `BVC label`：无溢出时分支
- `BHI label`：无符号更高时分支
- `BLS label`：无符号更低或相等时分支
- `BGE label`：有符号大于或等于时分支
- `BLT label`：有符号小于时分支
- `BGT label`：有符号大于时分支
- `BLE label`：有符号小于或等于时分支

### 64 位架构
- 条件分支指令类似：`B.EQ`、`B.NE` 等
- 另外还有比较并分支指令：`CBZ`（如果为零则跳转）、`CBNZ`（如果非零则跳转）
- 测试位并分支：`TBZ`（测试位为零跳转）、`TBNZ`（测试位非零跳转）

## 32 位与 64 位的主要差异对比

| 特性 | ARM 32 位 (AArch32) | ARM 64 位 (AArch64) |
|------|-------------------|-------------------|
| 寄存器数量 | 16 个 32 位通用寄存器 | 32 个 64 位通用寄存器 |
| 寄存器大小 | 32 位 | 64 位（兼容 32 位操作） |
| 地址空间 | 32 位地址（4GB） | 64 位地址（理论上 2^64） |
| 栈指针 | R13 (SP) | SP（独立的 64 位寄存器） |
| 链接寄存器 | R14 (LR) | X30 (LR) |
| 程序计数器 | R15 (PC) | PC（不能直接访问） |
| 状态寄存器 | CPSR | NZCV（分离的条件标志） |
| 指令编码 | 32 位固定长度（Thumb 为 16/32 位） | 32 位固定长度 |
| 函数参数传递 | R0-R3 用于参数 | X0-X7 用于参数 |
| 标志更新 | 大多数指令需 'S' 后缀更新标志 | 仅少数带 'S' 后缀的变体更新标志 |
| 条件执行 | 几乎所有指令可加条件后缀（如 ADDEQ） | 仅条件选择类指令（如 CSET/CSEL） |
| 大常数加载 | `MOVW`/`MOVT` | `MOVZ`/`MOVK` |
| 符号扩展加载 | `LDRSB`/`LDRSH` | `LDRSB`/`LDRSH`/`LDRSW` |

## 特殊功能指令

### 同步原语（32 位和 64 位都有）
- LDREX/STREX：独占加载/存储（AArch64 对应 LDXR/STXR）
- CLREX：清除独占监控
- SWP：交换（已废弃）

### 系统控制
- MSR：Move to System Register
- MRS：Move from System Register
- DMB：Data Memory Barrier（数据内存屏障）
- DSB：Data Synchronization Barrier（数据同步屏障）
- ISB：Instruction Synchronization Barrier（指令同步屏障）

### 编程示例对比

#### 32 位示例：加载不同数据类型
```armasm
.global _start
_start:
    MOVW R0, #0x1234      @ 设置低16位
    MOVT R0, #0x5678      @ 设置高16位，此时R0 = 0x56781234
    LDRB R1, [R2]         @ 加载无符号字节（零扩展到32位）
    LDRSB R3, [R4]        @ 加载有符号字节（符号扩展到32位）
    LDRH R5, [R6]         @ 加载无符号半字（零扩展到32位）
    LDRSH R7, [R8]        @ 加载有符号半字（符号扩展到32位）
    CMP R1, R2            @ 比较R1和R2，更新标志
    ORN R3, R4, R5        @ R3 = R4 OR NOT(R5)
    BEQ skip              @ 如果结果为零则跳过
    MOV R3, #20           @ 将立即数20放入R3
skip:
    B .                   @ 无限循环
```

#### 64 位示例：加载不同数据类型
```armasm
.global _start
_start:
    MOVZ X0, #0x1234             @ 设置最低16位
    MOVK X0, #0x5678, LSL #16   @ 设置次低16位
    MOVK X0, #0x9ABC, LSL #32   @ 设置次高16位
    MOVK X0, #0xDEF0, LSL #48   @ 设置最高16位
    LDRB W1, [X2]               @ 加载无符号字节（零扩展到32位）
    LDRSB X3, [X4]              @ 加载有符号字节（符号扩展到64位）
    LDRH W5, [X6]               @ 加载无符号半字（零扩展到32位）
    LDRSH X7, [X8]              @ 加载有符号半字（符号扩展到64位）
    LDRSW X9, [X10]             @ 加载32位（符号扩展到64位）
    CMP X1, X2                  @ 比较X1和X2，更新标志
    ORN X3, X4, X5             @ X3 = X4 OR NOT(X5)
    B.EQ skip                   @ 如果结果为零则跳过
    MOV X3, #20                 @ 将立即数20放入X3
skip:
    B .                         @ 无限循环
```

## 浮点运算指令

ARM 架构支持 IEEE 754 标准的浮点运算，主要通过 VFP（Vector Floating Point）和 NEON 单元实现。在 AArch64 中，这些功能由高级 SIMD（NEON）和浮点组件提供。

### 32 位架构中的浮点指令（VFPv3/VFPv4）

ARM 32 位架构中的浮点单元（FPU）通常实现了 VFPv3 或 VFPv4 扩展，提供了单精度（32 位）和双精度（64 位）浮点运算支持。

#### 浮点寄存器
- VFP 单元包含 32 个单精度浮点寄存器：S0-S31，或 16 个双精度寄存器：D0-D15
- 在 VFPv3-D32 模式下，可以访问 32 个双精度寄存器：D0-D31
- 也可以表示为 Q0-Q15（四字寄存器，128 位），用于 SIMD 操作
- 这些寄存器共享同一片物理存储区域：Dn 对应 S2n 和 S2n+1，Qn 对应 D2n 和 D2n+1

#### 浮点控制寄存器
- FPSCR（Floating Point Status and Control Register）：控制和状态寄存器
  - 包含累积异常标志、舍入模式、异常使能位和状态标志
  - 通过 VMRS/VMSR 指令访问

#### 浮点数据传输指令
- `VMOV Sd, Rt`：在通用寄存器和单精度浮点寄存器之间传送数据
- `VMOV Rt, Sd`：从单精度浮点寄存器向通用寄存器传送数据
- `VMOV.F32 Sd, #imm`：将（可编码的）浮点立即数加载到单精度浮点寄存器
- `VLDR Sd, [Rn, #offset]`：从内存加载单精度浮点数到寄存器
- `VSTR Sd, [Rn, #offset]`：将单精度浮点数存储到内存
- `VLDR Dd, [Rn, #offset]`：从内存加载双精度浮点数到寄存器
- `VSTR Dd, [Rn, #offset]`：将双精度浮点数存储到内存

#### 浮点算术运算指令
- `VADD.F32 Sd, Sn, Sm`：单精度浮点加法
- `VSUB.F32 Sd, Sn, Sm`：单精度浮点减法
- `VMUL.F32 Sd, Sn, Sm`：单精度浮点乘法
- `VDIV.F32 Sd, Sn, Sm`：单精度浮点除法
- `VNEG.F32 Sd, Sm`：单精度浮点取反
- `VABS.F32 Sd, Sm`：单精度浮点绝对值
- `VSQRT.F32 Sd, Sm`：单精度浮点平方根
- `VMLA.F32 Sd, Sn, Sm`：单精度浮点乘加（Sd = Sd + Sn * Sm）
- `VMLS.F32 Sd, Sn, Sm`：单精度浮点乘减（Sd = Sd - Sn * Sm）

> 双精度运算把后缀换成 `.F64` 并使用 D 寄存器，例如 `VADD.F64 Dd, Dn, Dm`。

#### 浮点比较指令
- `VCMP.F32 Sd, Sm`：单精度浮点比较
- `VCMP.F32 Sd, #0`：与 0 比较
- `VCMPE.F32 Sd, Sm`：单精度浮点比较（对信号 NaN 触发异常）
- 比较结果写入 FPSCR，需用 `VMRS APSR_nzcv, FPSCR` 传到条件标志后才能用于分支

#### 浮点转换指令
- `VCVT.S32.F32 Sd, Sm`：浮点到有符号整数转换（向零截断）
- `VCVT.U32.F32 Sd, Sm`：浮点到无符号整数转换
- `VCVT.F32.S32 Sd, Sm`：有符号整数到浮点转换
- `VCVT.F32.U32 Sd, Sm`：无符号整数到浮点转换
- `VCVT.F64.F32 Dd, Sm`：单精度到双精度转换
- `VCVT.F32.F64 Sd, Dm`：双精度到单精度转换

### 64 位架构中的浮点指令（AArch64）

AArch64 架构内置了完整的浮点和 SIMD 支持，提供了 32 个 128 位宽的寄存器（V0-V31），可用于浮点和 SIMD 操作。

#### 浮点寄存器表示
- V0-V31：128 位向量寄存器
- 可作为不同宽度使用：
  - 16 字节（128 位）：Qn
  - 8 字节（64 位）：Dn
  - 4 字节（32 位）：Sn
  - 2 字节（16 位）：Hn
  - 1 字节（8 位）：Bn
- 所有这些表示方法都引用同一个物理寄存器的不同部分

#### 浮点控制寄存器
- FPCR（Floating-Point Control Register）：浮点控制寄存器
  - 控制舍入模式、异常使能等
- FPSR（Floating-Point Status Register）：浮点状态寄存器
  - 包含异常标志、状态标志等

#### 浮点数据传输指令
- `FMOV Dd, Dn`：浮点寄存器间传送双精度数
- `FMOV Sd, Sn`：浮点寄存器间传送单精度数
- `FMOV Dd, Xn`：将通用寄存器的 64 位值传送到浮点寄存器
- `FMOV Xn, Dd`：将浮点寄存器的 64 位值传送到通用寄存器
- `FMOV Sd, Wn`：将通用寄存器的 32 位值传送到浮点寄存器
- `FMOV Wn, Sd`：将浮点寄存器的 32 位值传送到通用寄存器
- `LDR Sd, [Xn, #offset]`：加载单精度浮点数
- `STR Sd, [Xn, #offset]`：存储单精度浮点数
- `LDR Dd, [Xn, #offset]`：加载双精度浮点数
- `STR Dd, [Xn, #offset]`：存储双精度浮点数
- `LDR Qd, [Xn, #offset]`：加载 128 位向量数据

#### 浮点算术运算指令
- `FADD Sd, Sn, Sm`：单精度浮点加法
- `FSUB Sd, Sn, Sm`：单精度浮点减法
- `FMUL Sd, Sn, Sm`：单精度浮点乘法
- `FDIV Sd, Sn, Sm`：单精度浮点除法
- `FSQRT Sd, Sn`：单精度浮点平方根
- `FNEG Sd, Sn`：单精度浮点取反
- `FABS Sd, Sn`：单精度浮点绝对值
- `FMAX Sd, Sn, Sm`：单精度浮点最大值
- `FMIN Sd, Sn, Sm`：单精度浮点最小值
- `FNMUL Sd, Sn, Sm`：单精度浮点负乘法
- `FADD Dd, Dn, Dm`：双精度浮点加法
- `FSUB Dd, Dn, Dm`：双精度浮点减法
- `FMUL Dd, Dn, Dm`：双精度浮点乘法
- `FDIV Dd, Dn, Dm`：双精度浮点除法
- `FSQRT Dd, Dn`：双精度浮点平方根
- `FNEG Dd, Dn`：双精度浮点取反
- `FABS Dd, Dn`：双精度浮点绝对值
- `FMAX Dd, Dn, Dm`：双精度浮点最大值
- `FMIN Dd, Dn, Dm`：双精度浮点最小值

#### 浮点融合乘加指令
- `FMADD Sd, Sn, Sm, Sa`：单精度融合乘加（Sd = Sn × Sm + Sa）
- `FMSUB Sd, Sn, Sm, Sa`：单精度融合乘减（Sd = Sa - Sn × Sm）
- `FNMADD Sd, Sn, Sm, Sa`：单精度融合负乘加（Sd = -(Sn × Sm) - Sa）
- `FNMSUB Sd, Sn, Sm, Sa`：单精度融合负乘减（Sd = Sn × Sm - Sa）
- `FMADD Dd, Dn, Dm, Da`：双精度融合乘加
- `FMSUB Dd, Dn, Dm, Da`：双精度融合乘减
- `FNMADD Dd, Dn, Dm, Da`：双精度融合负乘加
- `FNMSUB Dd, Dn, Dm, Da`：双精度融合负乘减

#### 浮点比较指令
- `FCMP Sn, Sm`：单精度浮点比较
- `FCMP Dn, Dm`：双精度浮点比较
- `FCMPE Sn, Sm`：单精度浮点比较（对信号 NaN 触发异常）
- `FCMPE Dn, Dm`：双精度浮点比较（对信号 NaN 触发异常）

#### 浮点转换指令
- `SCVTF Sd, Wn`：32 位有符号整数到单精度浮点转换
- `SCVTF Dd, Wn`：32 位有符号整数到双精度浮点转换
- `SCVTF Sd, Xn`：64 位有符号整数到单精度浮点转换
- `SCVTF Dd, Xn`：64 位有符号整数到双精度浮点转换
- `UCVTF Sd, Wn`：32 位无符号整数到单精度浮点转换
- `UCVTF Dd, Wn`：32 位无符号整数到双精度浮点转换
- `UCVTF Sd, Xn`：64 位无符号整数到单精度浮点转换
- `UCVTF Dd, Xn`：64 位无符号整数到双精度浮点转换
- `FCVT Dd, Sn`：单精度到双精度转换
- `FCVT Sd, Dn`：双精度到单精度转换
- `FCVTZS Wd, Sn`：单精度浮点到 32 位有符号整数转换（向零截断）
- `FCVTZS Wd, Dn`：双精度浮点到 32 位有符号整数转换（向零截断）
- `FCVTZS Xd, Sn`：单精度浮点到 64 位有符号整数转换（向零截断）
- `FCVTZS Xd, Dn`：双精度浮点到 64 位有符号整数转换（向零截断）
- `FCVTZU Wd, Sn`：单精度浮点到 32 位无符号整数转换（向零截断）
- `FCVTZU Xd, Sn`：单精度浮点到 64 位无符号整数转换（向零截断）

### 浮点运算控制

ARM 浮点单元遵循 IEEE 754 标准，支持多种舍入模式：
- RN（Round to Nearest）：向最近值舍入（默认）
- RP（Round towards Plus Infinity）：向正无穷方向舍入
- RM（Round towards Minus Infinity）：向负无穷方向舍入
- RZ（Round towards Zero）：向零舍入

在 AArch64 中，可以通过 FPCR（浮点控制寄存器）配置舍入模式和其他浮点特性。

### 浮点编程示例

#### 32 位示例：简单的浮点计算
```armasm
.global _start
_start:
    VMOV.F32 S0, #2.0            @ S0 = 2.0
    VLDR S1, =3.5               @ 从内存加载3.5到S1
    VADD.F32 S2, S0, S1         @ S2 = S0 + S1 = 2.0 + 3.5 = 5.5
    VMUL.F32 S3, S2, S0         @ S3 = S2 * S0 = 5.5 * 2.0 = 11.0
    VCMP.F32 S3, S1            @ 比较S3和S1
    VMRS APSR_nzcv, FPSCR       @ 将浮点状态标志复制到普通状态标志
    BEQ equal_label             @ 如果相等则跳转
    B .                         @ 无限循环
equal_label:
    B .                         @ 无限循环
```

#### 64 位示例：简单的浮点计算
```armasm
.global _start
_start:
    FMOV S0, #2.0               @ S0 = 2.0
    LDR S1, =3.5                @ 从内存加载3.5到S1
    FADD S2, S0, S1             @ S2 = S0 + S1 = 2.0 + 3.5 = 5.5
    FMUL S3, S2, S0             @ S3 = S2 * S0 = 5.5 * 2.0 = 11.0
    FCMP S3, S1                 @ 比较S3和S1
    B.EQ equal_label            @ 如果相等则跳转
    B .                         @ 无限循环
equal_label:
    B .                         @ 无限循环
```

## 总结

ARM 从 AArch32 到 AArch64 的演进，体现了从“灵活但复杂”到“规整且高效”的设计取舍：

- **寄存器翻倍**：通用寄存器从 16 个增至 32 个，缓解寄存器压力、减少访存。
- **条件执行收敛**：AArch32 几乎所有指令都能加条件后缀，AArch64 收敛为条件选择类指令（CSEL/CSET 等），简化了指令译码与流水线。
- **标志更新简化**：AArch64 仅保留少数带 'S' 后缀的变体，移除了 `MOVS`/`ORRS`/`MULS`/`TEQ` 等。
- **浮点/SIMD 统一**：AArch64 用 32 个 128 位 V 寄存器统一管理标量浮点与向量运算。

理解两套架构的差异，是阅读跨平台二进制、做逆向分析与性能优化的基础。

## 相关架构

- [[21.Asm/12 MIPS.md|MIPS 汇编指令详解]]
- [[21.Asm/13 LoongArch64.md|LoongArch64 指令集详解]]
- [[21.Asm/15 x86(64).md|x86 汇编指令详解]]
- [[21.Asm/14 RISC-V.md|RISC-V 指令集详解]]
- [[21.Asm/01 汇编指令集对比.md|五架构对比总览]]
