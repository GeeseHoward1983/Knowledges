---
aliases: [启动流程与Bootloader, 启动流程, bootloader对比, 复位向量, 早期启动]
tags: [汇编, 指令集, 启动, bootloader, 固件, 裸机, 计算机体系结构, MOC]
---

# 启动流程与 Bootloader

从**上电复位**到**内核入口**，CPU 经历复位向量 → 固件 → bootloader → 内核的接力。本页对比五大架构的**复位状态、固件栈、早期启动汇编与内核传参**——是裸机系列（[[21.Asm/34 异常向量实战.md|34 异常向量]]、[[21.Asm/38 MMU与页表设置实战.md|38 MMU]]）的"最开头那一段"。

## 通用启动链

```
上电 → 复位向量（固定/CSR 指定）→ CPU 初始化 → 内存控制器初始化
     → 固件（BIOS/UEFI/TF-A/OpenSBI）→ bootloader（GRUB/U-Boot）
     → 内核入口（传 DTB/ACPI/EFI 配置）→ start_kernel
```

## 各架构复位与启动对照

| 架构 | 复位入口 | 复位特权态 | 地址翻译初态 | 固件栈 | 内核传参 |
|------|---------|-----------|-------------|--------|---------|
| [[21.Asm/15 x86(64).md\|x86]] | **`0xFFFFFFF0`** | 实模式 16 位 | 无分页 | BIOS/UEFI → GRUB | `boot_params`/EFI |
| [[21.Asm/11 ARM.md\|ARM64]] | `RVBAR`（实现定义） | 最高 EL（EL3/2/1） | MMU 关 | **TF-A** → U-Boot/UEFI | `x0`=DTB |
| [[21.Asm/14 RISC-V.md\|RISC-V]] | 实现定义 | **M 模式** | Bare（无翻译） | **OpenSBI** → U-Boot | `a0`=hartid, `a1`=DTB |
| [[21.Asm/13 LoongArch64.md\|LoongArch]] | CSR.EENTRY 体系 | PLV0 | **DMW 直接映射** | **UEFI** | `a0`=efi_boot, `a1`=cmdline, `a2`=systab |
| [[21.Asm/12 MIPS.md\|MIPS]] | **`0xBFC00000`** | 内核态 | kseg1 非缓存直映 | PMON/U-Boot | （平台约定） |

## 逐架构要点

### x86：三种模式逐级切换（最繁琐）

- 复位后 CPU 在 **16 位实模式**，从 **`0xFFFFFFF0`**（4GB 顶部下 16 字节）取第一条指令（通常是跳到 BIOS/UEFI）。
- 启动需**两次模式跃迁**：实模式 → **保护模式**（设 GDT、置 `CR0.PE`）→ **长模式**（建页表、置 `EFER.LME`、开 `CR0.PG`）。这是 x86 历史包袱的集中体现。
- 固件 BIOS/UEFI → GRUB → 内核（`boot_params` 或 EFI handover）。

### ARM64：从最高异常级逐级降

- 复位进入实现支持的**最高 EL**（常 EL3），`RVBAR_ELn` 指定复位向量；MMU 关、cache 视实现。
- **TF-A（Trusted Firmware-A）** 分阶段：BL1（ROM）→ BL2 → BL31（EL3 运行时）→ BL33（U-Boot/UEFI）→ 内核。固件逐级 `eret` 降到 **EL1** 跑内核（见 [[21.Asm/40 虚拟化扩展对比.md|EL 模型]]）。
- 内核入口 **`x0` = DTB 物理地址**。

### RISC-V：M 模式 + SBI

- 复位进入 **M 模式**（最高），复位 PC 实现定义。
- **OpenSBI** 作为 M 态运行时固件，向 S 态内核提供 **SBI（Supervisor Binary Interface）** 服务（定时器、IPI、控制台），再 `mret` 降到 **S 模式**启动内核。
- 内核入口 **`a0` = hartid（核号）、`a1` = DTB**；多核常约定一个 boot hart、其余 park 等待。

### LoongArch：UEFI + DMW 早映射（已核实）

- 现代 LoongArch（3A5000+）用 **UEFI 固件**（传统 PMON 已被取代，PMON 曾驻 `0x1c000000` flash 区）。
- 复位/异常入口由 **CSR.EENTRY** 体系控制（非 x86 式硬连地址）；**早期靠 DMW（直接映射窗口）**——上电即用 `0x9000…` 虚地址直接映射物理内存，**TLB/页表未建好也能跑**（见 [[21.Asm/38 MMU与页表设置实战.md|DMW]]）。
- 内核是 **EFI 镜像**，固件传参 **`a0`=efi_boot 标志、`a1`=cmdline、`a2`=EFI system table**；入口在 `head.S`。注意 IOCSR 须用 `iocsrrd`/`iocsrwr` 特殊指令访问（不映射物理地址空间）。

### MIPS：经典固定向量

- 复位从 **`0xBFC00000`**（kseg1，**非缓存直映**到物理 `0x1FC00000`）取指；处于内核态、MMU 对 kseg0/1 直映。
- 固件 PMON/U-Boot → 内核。MIPS 启动地址固定是其简洁之处。

## 早期启动汇编要做什么

无论哪家，复位后的第一段汇编（`head.S`/`start.S`）通常要：

1. **设栈指针**（`sp`），才能调用 C。
2. **清 `.bss` 段**（零初始化全局变量）。
3. **设异常向量**（`VBAR`/`mtvec`/`EENTRY`，见 [[21.Asm/34 异常向量实战.md|异常向量实战]]）。
4. **初始化 cache / MMU**（见 [[21.Asm/38 MMU与页表设置实战.md|MMU]]、[[21.Asm/39 缓存与缓存一致性.md|缓存]]）。
5. **建初始页表、开 MMU**（或用直映窗口过渡）。
6. **跳转 C 入口**（`start_kernel`/`main`）。

```asm
# 通用早期启动骨架（以 RISC-V 为例）
_start:
    la      sp, stack_top        # 1. 设栈
    la      t0, __bss_start      # 2. 清 BSS
    la      t1, __bss_end
1:  bgeu    t0, t1, 2f
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       1b
2:  la      t0, trap_vector      # 3. 设异常向量
    csrw    mtvec, t0
    # 4/5. （配 cache/MMU，可选）
    call    start_kernel         # 6. 跳 C
```

## 复位状态的关键差异

- **特权级**：x86 实模式、ARM 最高 EL、RISC-V M 模式、LoongArch PLV0、MIPS 内核态——都从最高权限起步，逐步降权跑内核。
- **地址翻译**：x86/ARM/RISC-V 复位时**无翻译/MMU 关**，要软件建页表后开启；**LoongArch 用 DMW 直接映射**、MIPS 用 kseg1 直映——这两家上电即可线性访问物理内存，少一道坎。
- **固件传参**：DTB（ARM/RISC-V 设备树）、ACPI/EFI（x86/LoongArch）——内核据此发现硬件。

## 总结

- 启动链统一为"复位向量 → 固件 → bootloader → 内核"，但**复位入口**（x86 `0xFFFFFFF0`、MIPS `0xBFC00000` 固定；ARM `RVBAR`、RISC-V 实现定义、LoongArch `CSR.EENTRY`）与**复位态**各异。
- 固件栈代表性：x86 BIOS/UEFI、ARM **TF-A**、RISC-V **OpenSBI**、LoongArch **UEFI**、MIPS PMON。
- 早期汇编六件事：设栈 → 清 BSS → 设异常向量 → 配 cache/MMU → 建页表开 MMU → 跳 C。
- **LoongArch DMW / MIPS kseg1** 让上电即可直映物理内存，是它们启动的便利；x86 三模式切换最繁琐。

## 相关页面

- [[21.Asm/34 异常向量实战.md|异常向量实战（裸机中断处理）]]
- [[21.Asm/38 MMU与页表设置实战.md|MMU / 页表设置实战]]
- [[21.Asm/39 缓存与缓存一致性.md|缓存与缓存一致性]]
- [[21.Asm/32 HelloWorld示例.md|各架构 Hello World 汇编示例]]
- [[21.Asm/00 总览.md|知识库总览]]
- [[21.Asm/11 ARM.md|ARM]]｜[[21.Asm/12 MIPS.md|MIPS]]｜[[21.Asm/13 LoongArch64.md|LoongArch]]｜[[21.Asm/14 RISC-V.md|RISC-V]]｜[[21.Asm/15 x86(64).md|x86]]
