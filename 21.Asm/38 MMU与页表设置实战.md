---
aliases: [MMU与页表设置实战, MMU设置, 页表设置, 虚拟内存设置, 页表实战]
tags: [汇编, 指令集, MMU, 页表, 虚拟内存, 裸机, 操作系统, 计算机体系结构, MOC]
---

# MMU / 页表设置实战

承接 [[21.Asm/34 异常向量实战.md|34 异常向量实战]] 的裸机系列，本页进入**虚拟内存**：MMU 如何把虚拟地址（VA）翻译成物理地址（PA），各架构**页表格式**与**启用 MMU 的步骤**有何不同。重点对比 x86-64 / ARM64 / RISC-V / LoongArch / MIPS 五家，并落到 RISC-V Sv39、LoongArch DMW 的实战骨架。

> 说明：示意骨架，省略页表内容构建细节；CSR/系统寄存器编号以各架构手册为准。

## MMU 核心概念

- **地址翻译**：VA 经**多级页表**逐级索引，得到 PA + 权限；结果缓存在 **TLB** 加速。
- **页表项（PTE）**：物理页帧号（PPN）+ 权限/属性位（有效、读写执行、用户态、访问、脏、缓存属性…）。
- **TLB**：翻译缓存，改页表后必须**失效（invalidate）** 对应项，否则用旧映射。

## 各架构页表格式对照

| 架构 | 页表基址寄存器 | 级数 | 页大小 | VA 位宽 | TLB 重填 |
|------|--------------|------|--------|---------|---------|
| [[21.Asm/15 x86(64).md\|x86-64]] | `CR3` | 4（PML4→PT），5（LA57） | 4K/2M/1G | 48（57） | 硬件 |
| [[21.Asm/11 ARM.md\|ARM64]] | `TTBR0/TTBR1_EL1` | 最多 4 | 4K/16K/64K 粒度 | 48（52） | 硬件 |
| [[21.Asm/14 RISC-V.md\|RISC-V]] | `satp` | Sv39=3 / Sv48=4 / Sv57=5 | 4K/2M/1G | 39/48/57 | 硬件 |
| [[21.Asm/13 LoongArch64.md\|LoongArch]] | `PGDL`/`PGDH` | 由 `PWCL`/`PWCH` 配 | 4K/16K… | 48 | 软件或硬件(HPTW) |
| [[21.Asm/12 MIPS.md\|MIPS]] | （无，软件 TLB） | 软件管理 | 可变 | — | **纯软件** |

> 两个分水岭：① **谁分半**——ARM `TTBR0/TTBR1`、LoongArch `PGDL/PGDH` 把低/高半地址空间用**两个根**分管（用户/内核天然隔离）；x86/RISC-V 用单根。② **谁填 TLB**——x86/ARM 硬件 walk；MIPS 纯软件（TLB 缺失即异常，软件填）；LoongArch 两者皆可（新核有硬件 walker HPTW）。

## 页表项权限位（PTE）

各家位名不同，语义相通：

- **x86-64**：`P`(present)、`R/W`、`U/S`(用户/特权)、`A`(accessed)、`D`(dirty)、`PS`(大页)、`NX`(不可执行)。
- **ARM64**：`Valid`、`AF`(access flag)、`AP`(权限)、`SH`(共享域)、`UXN/PXN`(用户/特权不可执行)、`nG`(非全局)。
- **RISC-V**：`V`(valid)、`R/W/X`、`U`(用户)、`G`(全局)、`A`、`D`；**`RWX=000` 表示该项指向下一级页表**而非叶。
- **LoongArch**（已核实）：`V`(valid)、`D`(dirty，决定写权限)、`PLV`(特权级)、`MAT`(cache 属性)、`G`(global)、`PRESENT`、`WRITE`、`NX/NR`(不可执行/读)、`RPLV`。

## 启用 MMU 的通用五步

1. **建页表**：在内存里填好各级目录与叶 PTE。
2. **设根**：把页表根物理地址写进基址寄存器（`CR3`/`TTBR`/`satp`/`PGDL`）。
3. **配转换控制**：页大小、VA 范围、ASID 等（`TCR`/`PWCL`-`PWCH`）。
4. **开 MMU**：置使能位（x86 `CR0.PG`、ARM `SCTLR_EL1.M`、RISC-V `satp.MODE≠0`、LoongArch `CRMD.PG=1,DA=0`）。
5. **同步**：屏障 + 失效旧 TLB（`isb`/`sfence.vma`/`invtlb`），确保新映射生效。

## RISC-V Sv39 实战

```asm
# 假设三级页表根已建好在 root_pt（物理地址）
    la      t0, root_pt
    srli    t0, t0, 12          # PPN = 根物理地址 >> 12
    li      t1, 8
    slli    t1, t1, 60          # MODE=8（Sv39）置于 satp[63:60]
    or      t0, t0, t1          # satp = MODE | (ASID=0) | PPN
    sfence.vma                  # 先清 TLB
    csrw    satp, t0            # 写 satp 即刻启用分页
    sfence.vma                  # 再次同步
```

> `satp`（RV64）= `MODE[63:60] | ASID[59:44] | PPN[43:0]`；MODE：0=Bare、8=Sv39、9=Sv48、10=Sv57。改页表后用 **`sfence.vma`**（可带 vaddr/asid 精确失效）。

## ARM64 实战骨架

```asm
    msr     TTBR0_EL1, x0       // 低半 VA（用户）页表根；TTBR1_EL1 管高半（内核）
    ldr     x1, =TCR_VALUE      // 配置粒度(TG)、VA 范围(T0SZ/T1SZ)、walk 属性
    msr     TCR_EL1, x1
    ldr     x1, =MAIR_VALUE     // 内存属性索引（设备/普通内存的 cache 属性）
    msr     MAIR_EL1, x1
    isb
    mrs     x1, SCTLR_EL1
    orr     x1, x1, #1          // SCTLR_EL1.M = 1，开 MMU
    msr     SCTLR_EL1, x1
    isb                         // 同步，之后取指/访存走翻译
```

> ARM64 的 `TTBR0/TTBR1` 按 VA 高位自动选择：用户态低半走 `TTBR0`、内核高半走 `TTBR1`，**切换进程只换 `TTBR0`**，内核映射常驻。

## LoongArch 实战要点（含直接映射窗口 DMW）

LoongArch 有个独门设计——**直接映射窗口（DMW）**：无需建页表即可线性访问物理内存，内核启动早期与常驻映射极方便：

```asm
    # 1) 配置 DMW0（CSR 0x180）：建立一段"VA 高位段 → PA"的直接映射
    li.d    $t0, 0x9000000000000011   # VSEG=0x9, MAT=cached, PLV0 使能
    csrwr   $t0, 0x180               # 之后访问 0x9000_xxxx_xxxx_xxxx
                                     # 直接映射到物理 0x0000_xxxx_xxxx_xxxx（清高 4 位）

    # 2) 页映射：设页目录根与遍历控制
    la.local $t0, pgd
    csrwr    $t0, 0x19               # PGDL（低半 VA 页目录根，0x19）
    li.d     $t0, PWCL_VALUE         # 各级 dir_base/dir_width + PTE 宽度
    csrwr    $t0, 0x1c               # PWCL（0x1c）
    li.d     $t0, PWCH_VALUE
    csrwr    $t0, 0x1d               # PWCH（0x1d）
    # STLBPS（0x1e）设 STLB 页大小；TLBRENTRY（0x88）设软件 TLB 重填入口

    # 3) 开分页：CRMD.PG=1、DA=0（从直接地址模式切到映射模式）
    csrrd    $t0, 0x0
    ori      $t0, $t0, (1 << 4)      # PG = bit4
    # 同时清 DA(bit3)…（略）
    csrwr    $t0, 0x0
```

> 核实要点：LoongArch 翻译有**两种模式并存**——`DMW0–3`（直接映射窗口，简单线性）与**页映射**（`PGDL/PGDH` 根 + `PWCL/PWCH` 遍历控制 + `STLB`/`MTLB`）。TLB 缺失经 `TLBRENTRY`（软件）或硬件 walker（HPTW，新核）。内核常用 `0x9000…`（cached）/`0x8000…`（uncached）两个 DMW，是 LoongArch 内核地址的由来。

## TLB 失效指令对照

| 架构 | 失效单页 | 失效全部 |
|------|---------|---------|
| x86 | `invlpg [addr]` | 重写 `CR3` |
| ARM64 | `tlbi vae1, x` + `dsb`/`isb` | `tlbi vmalle1` |
| RISC-V | `sfence.vma a0, a1`（vaddr,asid） | `sfence.vma`（无参） |
| LoongArch | `invtlb op, asid, vaddr` | `invtlb 0, $zero, $zero` |
| MIPS | `tlbwi`/`tlbp`（软件逐项管理） | 遍历重写 |

> 通用铁律：**改了页表，必须失效对应 TLB 项**，否则 CPU 继续用旧映射——这是 MMU 编程最隐蔽的 bug 来源。多核还需考虑 TLB shootdown（跨核失效，ARM `tlbi` 广播、x86 用 IPI）。

## 总结

- 五架构页表两大分水岭：**分半根**（ARM/LoongArch 的 `TTBR0/1`、`PGDL/H`）与 **TLB 重填**（x86/ARM 硬件、MIPS 纯软件、LoongArch 两者皆可）。
- 启用 MMU 五步通用：建表 → 设根 → 配控制 → 开使能位 → 屏障+失效 TLB。
- RISC-V 写 `satp` 即生效、ARM 置 `SCTLR.M`、LoongArch 切 `CRMD.PG`；LoongArch 的 **DMW 直接映射窗口**是其特色，免页表即可访物理内存。
- 改页表必失效 TLB（`sfence.vma`/`tlbi`/`invtlb`/`invlpg`），多核还要 TLB shootdown。

## 相关页面

- [[21.Asm/40 虚拟化扩展对比.md|虚拟化扩展对比]]
- [[21.Asm/34 异常向量实战.md|异常向量实战（裸机中断处理）]]
- [[21.Asm/29 异常中断处理对比.md|异常 / 中断处理对比]]
- [[21.Asm/28 原子操作与内存模型.md|原子操作与内存模型对比]]
- [[21.Asm/30 各架构性能特性对比.md|各架构性能特性对比]]
- [[21.Asm/00 总览.md|知识库总览]]
- [[21.Asm/11 ARM.md|ARM]]｜[[21.Asm/12 MIPS.md|MIPS]]｜[[21.Asm/13 LoongArch64.md|LoongArch]]｜[[21.Asm/14 RISC-V.md|RISC-V]]｜[[21.Asm/15 x86(64).md|x86]]
