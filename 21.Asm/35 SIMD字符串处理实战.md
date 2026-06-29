---
aliases: [SIMD字符串处理实战, SIMD字符串, 向量字符串处理, SIMD-strlen, 向量strlen]
tags: [汇编, 指令集, SIMD, 字符串, 向量, 示例代码, 计算机体系结构, MOC]
---

# SIMD 字符串处理实战

[[21.Asm/27 SIMD实战示例.md|27 SIMD 实战示例]] 讲通用数值向量化，本页专注 **SIMD 字符串处理**——`strlen`/`strchr`/`memchr`/大小写转换是 SIMD 最实用的战场。核心难点是一个看似简单的问题：**一次比较 16/32 字节后，怎么找到"第一个匹配字节"的位置**？五家给出的答案差异极大。

> 说明：示意代码（x86 用 AT&T 语法），省略对齐序言；重在讲清**比较→求掩码→定位**的结构与各架构差异。生产环境请优先用 libc（glibc 的 `strlen`/`memchr` 已高度 SIMD 优化并妥善处理跨页）。

## 核心套路

1. **批量加载**：一次取 16（SSE/NEON/LSX）或 32（AVX/LASX）或 VLEN（RVV）字节。
2. **并行比较**：逐字节与目标比较（`\0`、某字符、范围），匹配字节置全 1。
3. **求掩码 + 定位**：把"每字节是否匹配"压成紧凑信息，找**第一个**匹配位置。← **难点**
4. **处理对齐与尾部、跨页安全**：字符串长度未知，读越界是大坑。

## 难点：如何"找第一个匹配字节"

这一步五架构分野最大——关键看有没有 **movemask**（把每字节最高位收集成整数位掩码）：

| 架构 | 比较 | 求位置 | 难度 |
|------|------|--------|------|
| x86 SSE2 | `pcmpeqb` | **`pmovmskb`** → `bsf`/`tzcnt` | ⭐ 原生 movemask |
| ARM NEON | `cmeq` | 无 movemask → `shrn`+`fmov`+`rbit`+`clz` | ⭐⭐⭐ 要绕 |
| RISC-V RVV | `vmseq` | **`vfirst.m`**（+ `vle8ff` 跨页安全） | ⭐ 最干净 |
| LoongArch LSX | `vseqi.b` | **`vmskltz.b`** → `vpickve2gr`→ `ctz` | ⭐ 有 movemask 等价 |
| MIPS MSA | `ceq.b` | 无直接 movemask，类 NEON 绕 | ⭐⭐⭐ |

> `tzcnt`/`ctz` 见 [[21.Asm/25 位操作与移位指令对比.md|位操作]]。**x86 的 `pmovmskb` 是字符串 SIMD 的灵魂**；ARM 缺它要靠 `shrn` 技巧；RVV 的 `vfirst.m` 配合 `vle8ff` 最优雅。

## strlen 实战

### x86 SSE2（经典 pcmpeqb + pmovmskb）

```asm
# rdi = 字符串指针, 返回长度于 rax
strlen_sse2:
    mov      %rdi, %rax
    pxor     %xmm0, %xmm0           # xmm0 = 0（要找的 \0）
.loop:
    movdqu   (%rax), %xmm1          # 取 16 字节
    pcmpeqb  %xmm0, %xmm1           # 逐字节 ==0 → 匹配字节=0xFF
    pmovmskb %xmm1, %ecx            # 收集 16 个最高位 → 16 位掩码
    test     %ecx, %ecx
    jnz      .found
    add      $16, %rax
    jmp      .loop
.found:
    bsf      %ecx, %ecx             # 第一个置位 = \0 在本块内偏移
    sub      %rdi, %rax
    add      %rcx, %rax
    ret
```

### RISC-V RVV（vle8ff + vfirst，最优雅）

```asm
# a0 = 字符串指针, 返回长度
strlen_rvv:
    mv       a1, a0
.loop:
    vsetvli  t0, x0, e8, m8, ta, ma   # 尽量大的 vl（x0=要最大）
    vle8ff.v v8, (a1)                 # fault-only-first 加载：跨页越界自动缩短 vl
    csrr     t1, vl                   # ff 实际成功读入的字节数
    vmseq.vx v0, v8, x0               # 逐字节 ==0 → 掩码 v0
    vfirst.m a2, v0                   # mask 中第一个置位索引（无则 -1）
    bgez     a2, .found
    add      a1, a1, t1               # 未找到，前进 vl 个字节
    j        .loop
.found:
    sub      a1, a1, a0
    add      a0, a1, a2
    ret
```

> RVV 两大杀手锏：**`vle8ff.v`（fault-only-first）天然解决字符串跨页读**——越界只缩短 `vl` 而不报错；**`vfirst.m` 一条直接定位**，无需 movemask+ctz。这是 x86/ARM 都没有的、为变长向量量身定制的优雅。

### LoongArch LSX（vseqi.b + vmskltz.b，已核实）

```asm
# a0 = 指针, 返回长度
strlen_lsx:
    move      $t0, $a0
.loop:
    vld       $vr0, $t0, 0          # 取 16 字节
    vseqi.b   $vr0, $vr0, 0         # 逐字节 ==0 → 匹配字节=0xFF（符号位置1）
    vmskltz.b $vr1, $vr0            # 取每字节符号位 → 16 位掩码（pmovmskb 等价）
    vpickve2gr.hu $t1, $vr1, 0      # 把低 16 位掩码取到通用寄存器
    bnez      $t1, .found
    addi.d    $t0, $t0, 16
    b         .loop
.found:
    ctz.w     $t1, $t1             # 第一个置位
    sub.d     $t0, $t0, $a0
    add.d     $a0, $t0, $t1
    jr        $ra
```

### ARM NEON（无 movemask，shrn 技巧）

```asm
// ARM64 缺 pmovmskb，用 shrn 把 128 位比较结果压成 64 位定位
    ld1     {v0.16b}, [x0]
    cmeq    v0.16b, v0.16b, #0     // 逐字节 ==0
    shrn    v0.8b, v0.8h, #4       // 128→64：每字节压成 4 位 nibble
    fmov    x1, d0                 // 取到通用寄存器
    cbz     x1, .next              // 全 0 → 本块无 \0
    rbit    x1, x1
    clz     x1, x1                 // 找第一个匹配
    lsr     x1, x1, #2             // 每字节占 4 位，>>2 得字节偏移
```

> ARM 的 `shrn #4` 技巧是社区标志性 workaround：把每字节的比较结果（0x00/0xFF）压成半字节，再用 `rbit`+`clz` 定位。比 x86/LoongArch 的一条 movemask 多花好几条——这是 NEON 设计上缺 movemask 的实际代价（SVE 用谓词改善了此问题）。

## strchr / memchr 与大小写转换

- **strchr / memchr（找指定字符 c）**：把上面的"与 0 比较"换成"与 c 比较"即可——x86 `pcmpeqb` 一个广播了 c 的向量、RVV `vmseq.vx v0, v8, a1`（a1=c）、LSX `vseq.b` 与广播 c 的向量。`memchr` 还需结合长度做尾部边界。
- **大小写转换（toupper）**：对每字节做**范围判断**（`'a'..'z'`）再条件减 `0x20`。常用无分支技巧：用无符号比较把 `c-'a' < 26` 的字节挑出来生成掩码，`掩码 & 0x20` 后从原值减去。SIMD 一次处理 16/32 字节，远快于逐字节。

```asm
# 思路示意（x86）：把 [a-z] 转大写
    movdqu  (%rsi), %xmm0
    movdqa  %xmm0, %xmm2
    psubb   lower_a(%rip), %xmm2   # c - 'a'
    # 用无符号范围比较得到 [a-z] 的掩码，再 & 0x20，从 xmm0 减去
```

## 实战陷阱

1. **跨页读是头号坑**：长度未知却一次读 16/32 字节，可能跨过页边界读到**未映射页 → 段错误**。解法：① 先把指针**对齐**到向量宽度，首块用对齐加载（同一页内安全）；② **RVV `vle8ff` 天生免疫**——这是它相对 VLS 的实质优势。
2. **对齐更快**：`movdqa`（对齐）比 `movdqu`（未对齐）快，但要先用标量处理头部到对齐边界。
3. **尾部处理**：VLS（SSE/NEON/LSX）需要标量尾循环或掩码；**RVV 的 `vl` 让最后一块自动只处理剩余**，无尾循环（见 [[21.Asm/26 向量扩展对比.md|向量扩展对比]] 的 VLA 段）。
4. **别轻易重造轮子**：glibc 的字符串函数已是 SIMD + 跨页安全的高度优化版；手写 SIMD 仅在特殊场景（定长缓冲、自定义匹配）才划算。

## 总结

- 字符串 SIMD 的核心三步：**批量比较 → 求掩码 → 定位第一个匹配**；最大分野在"求掩码/定位"。
- **x86 `pmovmskb`、LoongArch `vmskltz.b`** 提供原生 movemask，一条出掩码再 `tzcnt`/`ctz`；**ARM NEON 缺 movemask**，靠 `shrn` 技巧绕；**RISC-V RVV `vfirst.m` + `vle8ff`** 既优雅又跨页安全，是 VLA 为字符串处理量身定制的范例。
- 跨页读与对齐是裸写 SIMD 字符串的最大陷阱，能用 libc 就用 libc。

## 相关页面

- [[21.Asm/27 SIMD实战示例.md|SIMD 实战示例：多架构向量化代码]]
- [[21.Asm/26 向量扩展对比.md|向量扩展对比：SIMD / 可变长向量]]
- [[21.Asm/25 位操作与移位指令对比.md|位操作与移位指令对比]]
- [[21.Asm/33 逆向反汇编速查.md|逆向 / 反汇编速查]]
- [[21.Asm/00 总览.md|知识库总览]]
- [[21.Asm/11 ARM.md|ARM]]｜[[21.Asm/12 MIPS.md|MIPS]]｜[[21.Asm/13 LoongArch64.md|LoongArch]]｜[[21.Asm/14 RISC-V.md|RISC-V]]｜[[21.Asm/15 x86(64).md|x86]]
