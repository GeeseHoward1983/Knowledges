---
aliases: [SVE与RVV进阶实战, 可变长向量进阶, VLA进阶, SVE进阶, RVV进阶]
tags: [汇编, 指令集, SIMD, 向量, VLA, SVE, RVV, 计算机体系结构, MOC]
---

# SVE / RVV 可变长向量进阶实战

[[21.Asm/26 向量扩展对比.md|26 向量扩展对比]] 讲了 VLS vs VLA 范式，[[21.Asm/27 SIMD实战示例.md|27 SIMD 实战]] 给了基础向量化。本页深入 **VLA（向量长度无关）** 的进阶编程：**谓词/掩码驱动的无尾循环、条件向量化、规约、gather/scatter**——并横向对比 ARM **SVE** 与 RISC-V **RVV** 两种 VLA 设计。

> 说明：示意代码，重在 VLA 编程范式。SVE 用谓词寄存器驱动、RVV 用 `vl` 驱动——这是两者最根本的区别。

## VLA 核心：循环不写死宽度

VLS（SSE/NEON/LSX）每轮处理**固定**条数、需尾循环；VLA 让**硬件决定每轮处理多少**：

- **RVV**：`vsetvli` 每轮申请 `vl`，循环按 `vl` 步进，最后一轮自动只处理剩余。
- **SVE**：`whilelo` 生成**循环谓词**（活跃通道掩码），非活跃通道自动跳过。

> ⚠️ 易混点：**`whilelo` 是 SVE 指令，不属于 RVV**；RVV 没有 "while" 类谓词生成指令，等价角色由 `vsetvli` 返回的 `vl` 承担。

## 无尾循环对照（数组加 `c[i]=a[i]+b[i]`）

```asm
// —— ARM SVE：谓词驱动 ——
// x0=a, x1=b, x2=c, x3=n（任意）
    mov     x4, #0
.loop:
    whilelo p0.s, x4, x3              // p0 = 活跃通道谓词（lane < n）
    ld1w    z0.s, p0/z, [x0, x4, lsl #2]   // 谓词加载（非活跃通道置0）
    ld1w    z1.s, p0/z, [x1, x4, lsl #2]
    fadd    z0.s, p0/m, z0.s, z1.s        // 谓词加（仅活跃通道）
    st1w    z0.s, p0,   [x2, x4, lsl #2]  // 谓词存（只写活跃通道）
    incw    x4                            // x4 += 每向量字数（VL 相关）
    b.first .loop                         // 还有活跃通道则继续
```

```asm
# —— RISC-V RVV：vl 驱动 ——
# a0=a, a1=b, a2=c, a3=n（任意）
.loop:
    vsetvli  t0, a3, e32, m1, ta, ma   # 本轮 vl=t0（≤剩余）
    vle32.v  v0, (a0)
    vle32.v  v1, (a1)
    vfadd.vv v2, v0, v1
    vse32.v  v2, (a2)
    sub      a3, a3, t0
    slli     t1, t0, 2
    add      a0, a0, t1
    add      a1, a1, t1
    add      a2, a2, t1
    bnez     a3, .loop
```

> 两者都**无尾循环**、同一二进制跨硬件宽度通用。SVE 用 `whilelo`+`incw`+`b.first` 控制循环、谓词管活跃通道；RVV 用 `vsetvli`+`vl` 管长度、手动步进指针。

## 条件向量化（ReLU：`y = max(x, 0)`）

含 `if` 的循环也能向量化——SVE 用谓词、RVV 用掩码 + `vmerge`：

```asm
// —— SVE：fmax 立即数，一条 ——
    ld1w    z0.s, p0/z, [x0, x4, lsl #2]
    fmax    z0.s, p0/m, z0.s, #0.0       // max(x,0)，谓词内逐通道
    st1w    z0.s, p0,   [x1, x4, lsl #2]
```

```asm
# —— RVV：比较生成掩码 v0，再 vmerge 选择 ——
    vle32.v   v8, (a0)              # x
    vmslt.vx  v0, v8, x0           # mask = (x < 0)，x0=zero
    vmv.v.i   v9, 0                # 0 向量
    vmerge.vvm v8, v8, v9, v0      # mask? 0 : x（vmerge 恒受 v0 掩码控制）
    vse32.v   v8, (a1)
```

> `vmerge.vvm vd, vs2, vs1, v0`：掩码位=1 取 `vs1`、=0 取 `vs2`，**始终**由 `v0` 驱动。SVE 谓词可直接修饰算术指令（`p0/m`），表达更紧凑。

## 规约（求和 `sum += a[i]`）

向量规约把一个向量"水平"折叠成标量——这是点积、范数、softmax 的核心：

```asm
# —— RVV：vredsum.vs 累加到标量 ——
    vmv.s.x   v8, x0               # 累加器 v8[0] = 0
.loop:
    vsetvli   t0, a1, e32, m1, ta, ma
    vle32.v   v0, (a0)
    vredsum.vs v8, v0, v8          # v8[0] = v8[0] + sum(v0[*])
    add       a0, a0, ...          # 步进
    bnez      a1, .loop
    vmv.x.s   a0, v8               # 取出标量和
```

```asm
// —— SVE：循环内谓词累加 + 循环后 faddv 一次 ——
    ptrue   p1.s                  // 全真谓词（供最终规约）
    mov     z1.s, #0              // 向量累加器
.loop:
    whilelo p0.s, x2, x1
    ld1w    z0.s, p0/z, [x0, x2, lsl #2]   // 非活跃通道读0
    fadd    z1.s, p1/m, z1.s, z0.s         // 加0安全，可用全谓词
    incw    x2
    b.first .loop
    faddv   s0, p1, z1.s          // 水平规约成标量
```

> RVV 规约语义：`vd[0] = vs1[0] ⊕ reduce(vs2[*])`，标量初值取自 `vs1[0]`，结果落 `vd[0]`。**浮点注意**：`vfredusum.vs`（无序，快）vs `vfredosum.vs`（有序，可复现）——RVV 1.0 把旧名 `vfredsum.vs` **重命名为 `vfredusum.vs`**。

## gather / scatter（间接索引）

按索引向量从分散地址取数（稀疏、查表、AoS 访问）：

```asm
// SVE gather：基址 + 索引向量
    ld1w    z0.s, p0/z, [x0, z4.s, sxtw #2]   // z0[i] = mem[x0 + z4[i]*4]
// RVV gather：无序索引加载
    vluxei32.v v8, (a0), v4                   // v8[i] = mem[a0 + v4[i]]
```

> RVV 另有 `vrgather`（寄存器内按索引重排）、`vlseg<nf>e<eew>.v`（**分段加载**，把内存中 AoS 拆进多个向量寄存器，实现 SoA 转换；注意分段访存在 RVV 1.0 **非强制**，部分实现省略）。SVE 对应有 `ld2/ld3/ld4` 结构化加载。

## SVE vs RVV 进阶对比

| 维度 | ARM SVE | RISC-V RVV |
|------|---------|------------|
| 长度控制 | **谓词** P0–P15（`whilelo` 生成） | **`vl`**（`vsetvli` 返回） |
| 掩码/谓词 | 16 个谓词寄存器，可直接修饰算术（`p/m`） | 掩码固定用 `v0`，多数指令带 `.vvm` |
| 寄存器分组 | 无 | **LMUL**（1/8–8，多寄存器拼成逻辑大向量） |
| 元素宽度 | 由指令后缀（`.b/.h/.s/.d`） | `vsetvli` 的 **SEW** |
| 配置 | `whilelo`/`ptrue`/`incb`/`incw` | `vsetvl`/`vsetvli`/`vsetivli` |
| 规约 | `faddv`/`addv`/`fadda` | `vredsum.vs`/`vfredusum.vs` |
| gather | `ld1w [base, z.idx]` | `vluxei`/`vrgather` |
| 宽度范围 | 128–2048 位（128 步进） | VLEN 可配（≥128，理论更大） |
| 余数处理 | 谓词自动 | `vl` 自动 |

> 哲学差异：**SVE 用"谓词"统一表达活跃通道与条件执行**，谓词是一等公民可直接挂在指令上；**RVV 用"`vl` + LMUL + `v0` 掩码"三件套**，`vl` 管长度、LMUL 管吞吐、`v0` 管条件。LMUL 是 RVV 独有的灵活点（一条指令吞吐可调）。

## 总结

- VLA 进阶的四大模式：**无尾循环、条件向量化、规约、gather/scatter**，SVE 与 RVV 各有惯用法。
- SVE 谓词（`whilelo`+P0–P15）可直接修饰指令、表达紧凑；RVV 靠 `vl`+`v0` 掩码+LMUL，`vsetvli` 一条配置长度与吞吐。
- 浮点规约注意 `vfredusum`（无序快）/`vfredosum`（有序可复现）之分；记住 `whilelo` 是 SVE 专属、RVV 用 `vl`。

## 相关页面

- [[21.Asm/26 向量扩展对比.md|向量扩展对比：SIMD / 可变长向量]]
- [[21.Asm/27 SIMD实战示例.md|SIMD 实战示例：多架构向量化代码]]
- [[21.Asm/35 SIMD字符串处理实战.md|SIMD 字符串处理实战]]
- [[21.Asm/24 浮点与FPU指令对比.md|浮点 / IEEE-754 与 FPU 指令对比]]
- [[21.Asm/00 总览.md|知识库总览]]
- [[21.Asm/11 ARM.md|ARM]]｜[[21.Asm/12 MIPS.md|MIPS]]｜[[21.Asm/13 LoongArch64.md|LoongArch]]｜[[21.Asm/14 RISC-V.md|RISC-V]]｜[[21.Asm/15 x86(64).md|x86]]
