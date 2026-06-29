---
aliases: [SIMD实战示例, SIMD代码示例, 向量化示例, SIMD实战, 向量化实战]
tags: [汇编, 指令集, SIMD, 向量, 示例代码, 计算机体系结构, MOC]
---

# SIMD 实战示例：同一计算的多架构向量化

本页用**同一段计算**在主流架构上的向量化实现，直观对比各家 SIMD 指令的写法差异，尤其是 **VLS（向量长度固定，需尾循环）** 与 **VLA（向量长度无关，自适应）** 在循环结构上的根本不同。配套概念见 [[21.Asm/26 向量扩展对比.md|向量扩展对比]]。

> 说明：以下均用 GNU as（GAS）语法，寄存器分配遵循各架构 64 位调用约定（见 [[21.Asm/23 调用约定对比.md|调用约定对比]]）。助记符以主流工具链为准；LoongArch LSX 指令已对照官方手册与社区资料核实，仍建议用对应 `-march`/`-msimd=lsx` 工具链实际汇编验证。

## 示例一：浮点数组逐元素相加

计算 `c[i] = a[i] + b[i]`（单精度 `float`）。这是最基础的「打包加法」，凸显各架构基本向量加 + 向量加载/存储的写法。

### x86 SSE（128 位，一次 4 个 float）

```asm
# rdi=a, rsi=b, rdx=c, rcx=n（n 为 4 的倍数）
.loop:
    movups  (%rdi), %xmm0      # 载入 4 个 float
    movups  (%rsi), %xmm1
    addps   %xmm1, %xmm0       # 4 路并行加：xmm0 += xmm1
    movups  %xmm0, (%rdx)
    add     $16, %rdi          # 步进 4×4=16 字节
    add     $16, %rsi
    add     $16, %rdx
    sub     $4, %rcx
    jnz     .loop
```

### x86 AVX（256 位，一次 8 个 float）

```asm
# 仅需把 xmm→ymm、步进 16→32、每轮 4→8；指令几乎照搬，这正是 VLS「换宽度=换指令」的体现
.loop:
    vmovups (%rdi), %ymm0      # 载入 8 个 float
    vaddps  (%rsi), %ymm0, %ymm0   # VEX 三操作数：ymm0 = ymm0 + [rsi]
    vmovups %ymm0, (%rdx)
    add     $32, %rdi
    add     $32, %rsi
    add     $32, %rdx
    sub     $8, %rcx
    jnz     .loop
```

### ARM NEON（128 位，一次 4 个 float）

```asm
// x0=a, x1=b, x2=c, x3=n
.loop:
    ld1     {v0.4s}, [x0], #16   // 载入 4 个 float，地址后递增 16
    ld1     {v1.4s}, [x1], #16
    fadd    v0.4s, v0.4s, v1.4s  // 4 路并行加
    st1     {v0.4s}, [x2], #16
    subs    x3, x3, #4
    b.ne    .loop
```

### RISC-V RVV（VLA：宽度由硬件决定，无尾循环）

```asm
# a0=a, a1=b, a2=c, a3=n（n 任意！）
.loop:
    vsetvli t0, a3, e32, m1, ta, ma  # 本轮处理 t0 个元素（≤剩余 a3，由硬件 VLEN 决定）
    vle32.v v0, (a0)                 # 载入 t0 个 float
    vle32.v v1, (a1)
    vfadd.vv v2, v0, v1              # 向量-向量加
    vse32.v v2, (a2)
    sub     a3, a3, t0               # 剩余元素数 -= 实际处理数
    slli    t1, t0, 2               # t0 × 4 字节
    add     a0, a0, t1
    add     a1, a1, t1
    add     a2, a2, t1
    bnez    a3, .loop
```

### LoongArch LSX（128 位，一次 4 个 float）

```asm
# a0=a, a1=b, a2=c, a3=n（n 为 4 的倍数）
.loop:
    vld     $vr0, $a0, 0       # 载入 4 个 float
    vld     $vr1, $a1, 0
    vfadd.s $vr0, $vr0, $vr1   # 打包单精度加
    vst     $vr0, $a2, 0
    addi.d  $a0, $a0, 16
    addi.d  $a1, $a1, 16
    addi.d  $a2, $a2, 16
    addi.d  $a3, $a3, -4
    bnez    $a3, .loop
```

### 关键对照

- **VLS（SSE / AVX / NEON / LSX）**：循环每轮处理**固定**条数（4 / 8 / …），要求 `n` 是向量宽度的整数倍，否则必须再写一段标量「尾循环」清理余数。换更宽硬件（128→256→512）几乎等于换一套指令并重新编译。
- **VLA（RVV）**：`vsetvli` 每轮向硬件申请「这次能处理多少」，返回实际 `vl` 到 `t0`，循环按 `t0` 步进——**天然消除尾循环**，同一份二进制在 128 位到上千位的不同 VLEN 硬件上都能直接跑（参见 [[21.Asm/26 向量扩展对比.md|向量扩展对比]] 的 VLA 段）。

## 示例二：SAXPY（`y = a·x + y`）

`a` 是标量，`x`/`y` 是向量。这个 BLAS 经典核函数同时考察两件事：**标量广播**（把 `a` 复制到所有通道）与 **FMA（乘加融合）**。

### x86 AVX2 + FMA（256 位）

```asm
# 入口前：ymm15 = broadcast(a)，rdi=x, rsi=y, rcx=n
# 例如 vbroadcastss (%rax), %ymm15 把内存中的 a 广播到 8 个通道
.loop:
    vmovups     (%rdi), %ymm0          # x
    vfmadd213ps (%rsi), %ymm15, %ymm0  # ymm0 = ymm15*ymm0 + [rsi] = a*x + y
    vmovups     %ymm0, (%rsi)
    add         $32, %rdi
    add         $32, %rsi
    sub         $8, %rcx
    jnz         .loop
```

> FMA3 的 `213` 后缀表示运算次序：`vfmadd213ps d, s2, s3` 计算 `d = s2*d + s3`。需 CPU 支持 FMA（Haswell 及以后）。

### ARM NEON（128 位）

```asm
// 入口前：v31 = dup a（dup v31.4s, v0.s[0] 把 s0 中的 a 复制到 4 通道）
// x0=x, x1=y, x2=n
.loop:
    ld1     {v0.4s}, [x0], #16   // x
    ld1     {v1.4s}, [x1]        // y（不递增，下面要写回同址）
    fmla    v1.4s, v0.4s, v31.4s // v1 += v0*v31 → y += x*a
    st1     {v1.4s}, [x1], #16
    subs    x2, x2, #4
    b.ne    .loop
```

### RISC-V RVV（标量直接进 FMA，最简洁）

```asm
# fa0 = a（标量浮点寄存器，无需显式广播）；a0=x, a1=y, a2=n
.loop:
    vsetvli t0, a2, e32, m1, ta, ma
    vle32.v v0, (a0)             # x
    vle32.v v1, (a1)             # y
    vfmacc.vf v1, fa0, v0       # v1 += fa0*v0 → y += a*x（.vf = 向量×标量）
    vse32.v v1, (a1)
    sub     a2, a2, t0
    slli    t1, t0, 2
    add     a0, a0, t1
    add     a1, a1, t1
    bnez    a2, .loop
```

> RVV 的 `.vf` 变体直接吃浮点寄存器里的标量，省去单独的广播步骤——这是变长向量在「向量 op 标量」场景下的便利。

### LoongArch LSX（128 位，FMA 为四操作数）

```asm
# 入口前：vr31 = broadcast(a)，可用 vldrepl.w $vr31, $a4, 0 从内存载入并复制到 4 通道
# a0=x, a1=y, a2=n
.loop:
    vld      $vr0, $a0, 0       # x
    vld      $vr1, $a1, 0       # y
    vfmadd.s $vr1, $vr0, $vr31, $vr1   # vr1 = vr0*vr31 + vr1 = x*a + y
    vst      $vr1, $a1, 0
    addi.d   $a0, $a0, 16
    addi.d   $a1, $a1, 16
    addi.d   $a2, $a2, -4
    bnez     $a2, .loop
```

> LSX/LASX 沿袭 MIPS 的「非破坏性」风格，FMA 用**四个寄存器字段**（`vd = vj*vk + va`），等价于 x86 的 FMA4 形态，不必覆盖某个源操作数。

## 总结对照表

| 维度 | x86 SSE/AVX | ARM NEON | RISC-V RVV | LoongArch LSX |
|------|-------------|----------|------------|---------------|
| 打包加 | `addps`/`vaddps` | `fadd v.4s` | `vfadd.vv` | `vfadd.s` |
| FMA | `vfmadd213ps`（FMA4 三操作数语义） | `fmla`（累加进目标） | `vfmacc.vf`（吃标量） | `vfmadd.s`（四操作数） |
| 标量广播 | `vbroadcastss` | `dup v.4s, v.s[0]` | 无需（`.vf` 直接吃标量） | `vldrepl.w` / `vreplvei.w` |
| 加载后递增 | 否（手动 `add`） | 是（`[x0], #16`） | 否（手动 `add`） | 否（手动 `addi.d`） |
| 循环结构 | **定长**，需尾循环 | **定长**，需尾循环 | **变长**，`vsetvli` 自适应 | **定长**，需尾循环 |

**一句话总结**：VLS 架构（x86 / NEON / LSX）写法相似——每轮处理固定条数、靠手动或后递增步进、需要尾循环；RVV 用 `vsetvli` 把「每轮处理多少」交给硬件，循环天然无尾、跨宽度通用，这正是 VLA 范式最直观的实战收益。

## 相关页面

- [[21.Asm/35 SIMD字符串处理实战.md|SIMD 字符串处理实战]]
- [[21.Asm/26 向量扩展对比.md|向量扩展对比：SIMD / 可变长向量]]
- [[21.Asm/36 SVE与RVV进阶实战.md|SVE / RVV 进阶实战]]
- [[21.Asm/24 浮点与FPU指令对比.md|浮点 / IEEE-754 与 FPU 指令对比]]
- [[21.Asm/23 调用约定对比.md|调用约定 / ABI 横向对照]]
- [[21.Asm/01 汇编指令集对比.md|五架构对比总览]]
- [[21.Asm/11 ARM.md|ARM]]｜[[21.Asm/12 MIPS.md|MIPS]]｜[[21.Asm/13 LoongArch64.md|LoongArch]]｜[[21.Asm/14 RISC-V.md|RISC-V]]｜[[21.Asm/15 x86(64).md|x86]]
