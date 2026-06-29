---
aliases: [浮点与FPU指令对比, IEEE-754对比, 浮点指令对比, FPU对比, 浮点比较对比]
tags: [汇编, 指令集, 浮点, IEEE-754, FPU, 计算机体系结构, MOC]
---

# 浮点 / IEEE-754 与 FPU 指令对比

本页先讲**架构无关**的 IEEE-754 共同地基，再横向对比五大架构的 FPU 寄存器、控制状态与标量浮点指令。最值得玩味的是**浮点比较结果"存哪儿"**——五家给出了五种答案，正是整数标志位之争（见 [[21.Asm/01 汇编指令集对比.md|对比总览]]）在浮点世界的延续。

## IEEE-754 基础（共同地基）

所有现代架构的浮点都遵循 IEEE-754，差异只在指令外壳。

### 格式

| 名称 | 位宽 | 符号 | 指数 | 尾数 | 指数偏移 | 对应类型 |
|------|------|------|------|------|---------|---------|
| binary16（半） | 16 | 1 | 5 | 10 | 15 | `_Float16` |
| binary32（单） | 32 | 1 | 8 | 23 | 127 | `float` |
| binary64（双） | 64 | 1 | 11 | 52 | 1023 | `double` |
| 80 位扩展（x87） | 80 | 1 | 15 | 64（显式整数位） | 16383 | x86 `long double` |
| binary128（四） | 128 | 1 | 15 | 112 | 16383 | `__float128` |

> 值 = (−1)^符号 × 1.尾数 × 2^(指数−偏移)。规格化数有隐含前导 1；x87 的 80 位格式是唯一**显式**存整数位的。

### 特殊值与舍入

- **特殊值**：±0、±∞、**NaN**（qNaN 静默 / sNaN 信号）、**非规格化数**（subnormal，指数全 0，填补 0 附近的空隙）。
- **5 种舍入模式**（用 RISC-V `frm` 命名直观）：`RNE` 就近舍入到偶数（默认）、`RTZ` 向零（截断）、`RDN` 向 −∞、`RUP` 向 +∞、`RMM` 就近且平局远离零。
- **5 个异常标志**：`NV` 无效操作、`DZ` 除零、`OF` 上溢、`UF` 下溢、`NX` 不精确。各架构都把它们攒在浮点控制状态寄存器里。

## 五架构 FPU 概览

| 架构 | 浮点单元 | 浮点寄存器 | 控制/状态 | 比较结果去向 | 半精度 |
|------|---------|-----------|----------|-------------|--------|
| [[21.Asm/15 x86(64).md\|x86]] | x87 + SSE/AVX | x87 栈 ST(0–7)（80 位）/ XMM/YMM/ZMM | x87 FPCW/FPSW + **MXCSR** | **EFLAGS** | F16C/AVX-512 |
| [[21.Asm/11 ARM.md\|ARM]] | VFP + AdvSIMD/SVE | V0–V31（标量视图 H/S/D） | **FPCR/FPSR** | **NZCV** | 原生支持 |
| [[21.Asm/14 RISC-V.md\|RISC-V]] | F/D/Q/Zfh 扩展 | f0–f31 | **fcsr**（frm+fflags） | **整数寄存器** | Zfh 扩展 |
| [[21.Asm/13 LoongArch64.md\|LoongArch]] | 基础浮点 | f0–f31 | **fcsr0–3** + fcc0–7 | **fcc 条件标志** | — |
| [[21.Asm/12 MIPS.md\|MIPS]] | FPU（CP1） | $f0–$f31 | **FCSR** | **FCC 条件码位** | MIPS-3D |

> LoongArch 有 **8 个 1 位条件标志 fcc0–fcc7** 与 4 个 fcsr（严格说仅 fcsr0 真实存在，fcsr1–3 是其字段别名）。

## x87 的历史包袱

x86 最初的浮点是 **x87 栈式协处理器**：8 个 80 位寄存器组成栈 `ST(0)–ST(7)`，用 `fld`/`fstp`/`faddp` 等**栈操作**，编程别扭、调度困难。**x86-64 时代标量浮点已全面改用 SSE**（`addss`/`addsd` 作用于 XMM），规整得多；x87 如今主要只为 80 位 `long double` 保留。这是「指令集历史包袱」的典型，读老代码/反汇编时仍会遇到。

## 标量浮点指令对照

| 操作 | x86 SSE | ARM64 | RISC-V | LoongArch | MIPS |
|------|---------|-------|--------|-----------|------|
| 加（单/双） | `addss`/`addsd` | `fadd s/d` | `fadd.s`/`fadd.d` | `fadd.s`/`fadd.d` | `add.s`/`add.d` |
| 乘 | `mulss`/`mulsd` | `fmul` | `fmul.s/.d` | `fmul.s/.d` | `mul.s/.d` |
| 除 | `divss`/`divsd` | `fdiv` | `fdiv.s/.d` | `fdiv.s/.d` | `div.s/.d` |
| 平方根 | `sqrtss`/`sqrtsd` | `fsqrt` | `fsqrt.s/.d` | `fsqrt.s/.d` | `sqrt.s/.d` |
| FMA | `vfmadd…ss` | `fmadd` | `fmadd.s/.d` | `fmadd.s/.d` | `madd.s/.d` |
| 整→浮 | `cvtsi2ss` | `scvtf` | `fcvt.s.w` | `ffint.s.w` | `cvt.s.w` |
| 浮→整（截断） | `cvttss2si` | `fcvtzs` | `fcvt.w.s`（带 rtz） | `ftintrz.w.s` | `trunc.w.s` |
| 寄存器搬移（浮↔整） | `movd`/`movq` | `fmov` | `fmv.x.w`/`fmv.w.x` | `movfr2gr`/`movgr2fr` | `mfc1`/`mtc1` |

## 浮点比较：五种"结果存哪儿"（核心分野）

同样是「比较 a 与 b 再分支」，五家把比较结果放到了五个不同的地方——这是各自标志位哲学的直接体现：

```asm
# —— x86 SSE：结果进通用标志 EFLAGS(ZF/PF/CF)，复用整数条件跳转 ——
    ucomiss %xmm1, %xmm0       # 比较，设 EFLAGS
    jb      less_label         # 借整数条件跳转

# —— ARM64：结果进通用标志 NZCV，复用整数条件分支 ——
    fcmp    s0, s1             # 设 PSTATE.NZCV
    b.lt    less_label

# —— RISC-V：结果(0/1)写入整数寄存器，再用整数分支（与"整数无标志"一致）——
    flt.s   t0, fa0, fa1       # t0 = (fa0 < fa1) ? 1 : 0
    bnez    t0, less_label     # 还有 feq.s / fle.s

# —— LoongArch：结果写入专用浮点条件标志 fcc0-7，再用浮点分支 ——
    fcmp.clt.s $fcc0, $fa0, $fa1   # 若 fa0<fa1 则 fcc0=1（cond 有 ceq/clt/cle/cun… 22 种）
    bcnez   $fcc0, less_label      # 测 fcc 分支（bceqz/bcnez）

# —— MIPS（传统）：结果写 FCSR 的 FCC 条件码位，再用 bc1t/bc1f ——
    c.lt.s  $f0, $f1           # 设 FCC（MIPS R6 改为 cmp.lt.s 写浮点寄存器 + bc1nez）
    bc1t    less_label
```

| 架构 | 比较指令 | 结果去向 | 分支方式 |
|------|---------|---------|---------|
| x86 SSE | `ucomiss`/`comiss` | 通用标志 EFLAGS | `ja`/`jb`/`je`… |
| ARM64 | `fcmp` | 通用标志 NZCV | `b.lt`/`b.gt`… |
| RISC-V | `feq/flt/fle.s` | **整数寄存器** | `bnez`/`beqz` |
| LoongArch | `fcmp.cond.s` | **浮点条件标志 fcc** | `bcnez`/`bceqz` |
| MIPS | `c.cond.s`（传统） | **FPU 内 FCC 位** | `bc1t`/`bc1f` |

> 规律呼应整数世界：x86/ARM 复用通用标志位；RISC-V 贯彻"无标志、写寄存器"的极简哲学；LoongArch/MIPS 给浮点单设条件标志。LoongArch 还有条件选择 `fsel fd, fj, fk, fcc`（`fcc==0 ? fj : fk`），免分支。

## FMA：融合乘加

`a×b+c` 若用「乘 + 加」两条指令会**舍入两次**；**FMA（fused multiply-add）一条指令、只在最后舍入一次**，精度更高、吞吐更好，是点积/矩阵/多项式求值的主力：

- x86：`vfmadd213ss` 等（FMA3，Haswell+）；ARM64：`fmadd`（4 操作数 `fd=fn*fm+fa`）；
- RISC-V：`fmadd.s/.d`（外加 `fmsub`/`fnmadd`/`fnmsub`）；LoongArch：`fmadd.s/.d`（四操作数，非破坏性）；MIPS：`madd.s/.d`。

## 转换与舍入控制

浮点↔整数转换、单↔双精度转换，是 bug 高发区，关键看**用哪种舍入**：

- **RISC-V**：`fcvt.w.s` 默认按 `fcsr.frm`，也可指令内显式指定后缀（`rne`/`rtz`/`rdn`/`rup`/`rmm`）。
- **LoongArch**：`ftintrz.w.s` 固定向零、`ftint.w.s` 用当前舍入模式、`ffint.s.w` 整→浮（这些助记符曾是 LoongArch 笔记的纠错重点，已核实）。
- **x86**：`cvttss2si` 的双 `t` 表示**截断**（向零），`cvtss2si` 则按 MXCSR；ARM64 `fcvtzs`（向零）vs `fcvtns`（就近）等后缀直接编码舍入。

> 通用陷阱：「浮点转整数」C 语义是**向零截断**，但「按当前舍入模式」的转换指令默认可能是就近——选错指令会让边界值差 1。

## 总结

- IEEE-754 是共同地基（格式 / 特殊值 / 5 种舍入 / 5 个异常标志），各架构差异只在指令外壳与状态寄存器。
- **浮点比较是最大分野**：x86→EFLAGS、ARM→NZCV、RISC-V→整数寄存器、LoongArch→fcc、MIPS→FCC，五种设计一字排开，正是标志位哲学的浮点版。
- 优先用 **FMA** 提升精度与吞吐；做浮点→整数转换务必确认**舍入模式**（截断 vs 就近）。
- x87 是历史包袱，现代 x86-64 标量浮点走 SSE；半精度在 ARM/RISC-V(Zfh)/x86(F16C) 上各有支持。

## 相关页面

- [[21.Asm/26 向量扩展对比.md|向量扩展对比：SIMD / 可变长向量]]
- [[21.Asm/27 SIMD实战示例.md|SIMD 实战示例：多架构向量化代码]]
- [[21.Asm/30 各架构性能特性对比.md|各架构性能特性对比]]
- [[21.Asm/25 位操作与移位指令对比.md|位操作与移位指令对比]]
- [[21.Asm/21 寻址模式专题.md|寻址模式专题]]
- [[21.Asm/01 汇编指令集对比.md|五架构对比总览]]
- [[21.Asm/11 ARM.md|ARM]]｜[[21.Asm/12 MIPS.md|MIPS]]｜[[21.Asm/13 LoongArch64.md|LoongArch]]｜[[21.Asm/14 RISC-V.md|RISC-V]]｜[[21.Asm/15 x86(64).md|x86]]
