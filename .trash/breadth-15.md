# 选题广度审查报告：15 操作系统加载与启动

审查日期：2026-07-01
审查员：广度审查 Agent

---

## 一、现有 13 篇覆盖摘要

| # | 篇名 | 核心覆盖 |
|---|---|---|
| 01 | 加电到用户态的启动全景 | 接力链鸟瞰（固件→引导→内核→init→用户态） |
| 02 | BIOS 与 UEFI 固件 | BIOS/POST、UEFI ESP/PE32+/Boot/Runtime Services、Secure Boot 固件基础 |
| 03 | MBR 与 GPT 与引导扇区 | MBR 512B/0x55AA、分区表格式、GPT 布局、引导阶段 |
| 04 | 引导加载器 GRUB 与 U-Boot | GRUB2 多阶段（boot.img→core.img→stage2）、UEFI 模式、U-Boot SPL/DTB/bootcmd |
| 05 | 从实模式到保护模式到长模式 | GDT、CR0.PE/EFER.LME、A20 线、16→32→64 位 CPU 状态转换 |
| 06 | 内核解压与早期初始化 | bzImage 格式、startup_32→startup_64→decompress→start_kernel、KASLR |
| 07 | 分页与页表机制 | x86-64 四级页表、CR3、PTE 标志、TLB、巨页 |
| 08 | 虚拟内存子系统 | 按需调页、缺页 #PF、COW、swap、OOM |
| 09 | 中断异常与 IDT | IDT 256 项、向量 0–31、APIC/IRQ、中断门 |
| 10 | 系统调用机制 | syscall/sysret、LSTAR/STAR、int 0x80、vDSO |
| 11 | init 与 systemd 启动 | PID 1 诞生、initramfs/switch_root、systemd unit/target/依赖图并行化 |
| 12 | ELF 程序的加载与执行 | execve→load_elf_binary、PT_LOAD/PT_INTERP、auxv |
| 13 | 可信启动与启动安全 | Secure Boot（PK/KEK/db/dbx/shim/MOK）、Measured Boot/TPM PCR、dm-verity、IMA/EVM、Bootkit 防御 |

**整体评估**：基准平台（x86-64 + Linux + UEFI/GRUB）已覆盖完整；但多平台启动路径（Windows、macOS、Android）、特殊启动模式（休眠恢复、kexec）、替代固件（coreboot）、容器/命名空间层启动及深度 initramfs 机制均未独立成篇。

---

## 二、建议新增篇目

### A. Windows 启动链

**拟标题**：Windows 启动链：BOOTMGR / winload / 内核初始化

**价值**：Windows bootmgr → winload.efi → ntoskrnl 与 Linux GRUB → vmlinuz 路径完全不同，涵盖 BCD（Boot Configuration Data）存储、hibernation 文件（hiberfil.sys）、Driver Signing、Early Launch Anti-Malware（ELAM）、PatchGuard 初始化。对 Windows 逆向/安全工程师和比较系统研究者是不可缺的核心知识。

**关键子话题**：BCD 结构与 bcdedit 操作；winload 的内核/HAL 加载序列；ELAM 驱动在内核初始化最早期的回调；PatchGuard（KPP）的初始化时机；BitLocker 在 TPM 封存与 winload 阶段的解封流程；WinPE 引导。

---

### B. macOS / Apple Silicon 启动链

**拟标题**：macOS 启动链：iBoot / boot.efi / kernelcache（从 Intel 到 Apple Silicon）

**价值**：Apple 平台形成从 SecureROM（芯片固化只读）→iBoot→XNU 的封闭但公开文档完整的可信启动链。Intel Mac 走 boot.efi+kernelcache，Apple Silicon 走 iBoot+SFR（Secure Firmware Runtime），两条路径对 iOS/macOS 逆向工程师、Jailbreak/越狱研究者、嵌入式安全工程师价值极高；且与现有 Linux 路径形成鲜明对比。

**关键子话题**：SecureROM 角色与 DFU 模式；iBoot 验签策略（iOS LLB/iBoot/kernelcache）；kernelcache（prelinked kernel）格式；Apple Silicon 的 Secure Enclave 在启动中的角色；bootargs/DeviceTree；LocalPolicy 与 SFR。

---

### C. Android 启动链（ABL / AVB / vbmeta）

**拟标题**：Android 启动链：ABL / vbmeta / AVB 2.0 / dm-verity

**价值**：Android 在嵌入式 ARM 启动链上叠加了 Verified Boot 2.0（AVB）专有机制，与 Linux dm-verity 虽同源但自成体系：ABL（Android Bootloader）→ bootloader 解锁状态→ vbmeta 链签名验证→ dm-verity 保护 system/vendor。是 Android 逆向、刷机工具链开发、安全研究的核心前置知识，与现有第 04 篇（U-Boot）和第 13 篇（dm-verity 简介）均不重叠。

**关键子话题**：ABL 与 fastboot 协议；bootloader 锁定/解锁对 AVB 的影响；vbmeta 链（vbmeta→boot→system→vendor）；AVB Footer 与哈希描述符；rollback protection（防降级）机制；ramdisk/first stage init；A/B 分区更新方案（OTA）。

---

### D. 休眠与恢复（Hibernation / S3 / S4）

**拟标题**：休眠与恢复：ACPI S3/S4、hibernation 镜像与内核恢复路径

**价值**：S3（挂起到内存）和 S4（挂起到磁盘/hibernation）是操作系统与硬件协同的"第二次启动"——S4 需要把整个内存快照写入 swap/hiberfil.sys，再由引导加载器在下次开机时识别并加载恢复内核。Linux swsusp/uswsusp 路径、Windows hibernation 恢复在 winload 阶段的特殊分支、UEFI ACPI 在 S3 resume 时的硬件重初始化序列，对系统工程师和固件/驱动开发者极有价值；也是取证工程师提取内存镜像的重要攻击面。

**关键子话题**：ACPI 电源状态 G0–G3 与 S 状态定义；S3 resume 的 UEFI/BIOS 路径（不通过 bootloader）；S4/hibernation 镜像格式（Linux swsusp header、Windows hiberfil.sys 结构）；内核恢复路径（`swsusp_restore`）；KASLR 与恢复的交互；EFI 变量在 S3 resume 中的角色。

---

### E. kexec 快速重启与内核热替换

**拟标题**：kexec：绕过固件的内核热替换与 kdump 崩溃转储

**价值**：`kexec` 是 Linux 独有的机制——用正在运行的内核直接把新内核加载进内存并跳转，完全绕过 BIOS/UEFI 和 bootloader，重启速度可快 10–30 秒。它也是 **kdump**（内核崩溃转储）的核心基础设施：crash kernel 预先由 kexec 保留内存，在主内核 panic 时接管并把内存镜像写入磁盘。对 Linux 内核开发者、SRE、崩溃分析工程师是日常工具；也是启动链安全研究中一个绕过 Secure Boot 的有趣角度。

**关键子话题**：kexec_load/kexec_file_load 系统调用；内核跳转的段重置序列；kdump crashkernel 预留内存；vmcore 格式与 crash 分析工具；Secure Boot 下 kexec 的签名要求（kexec_file_load 走内核签名验证）；kexec 与 systemd-kexec 集成。

---

### F. initramfs 深入：dracut / mkinitcpio 工作原理

**拟标题**：initramfs 深入：构建原理、dracut / mkinitcpio、early userspace 调试

**价值**：现有第 11 篇对 initramfs 仅做了"cpio 解包 + switch_root"的概要描述，而 initramfs 的**构建机制**（哪些驱动被打包、如何决策、hook 体系）和**调试技巧**（rd.break、dracut rescue shell、systemd-analyze）才是工程师日常排障的核心。initramfs 阶段是许多启动故障的高发区（LUKS 密钥提示、LVM 激活、iSCSI 挂载、网络引导），值得独立深挖。

**关键子话题**：cpio 归档格式与内核解包位置；dracut 模块体系与 hook 点（pre-udev / pre-mount / pre-pivot）；mkinitcpio HOOKS 数组；early userspace 中的 udev 触发序列；rd.break 调试断点；systemd 在 initramfs 阶段的特殊 target（initrd.target）；网络引导（iPXE + NFS root）的 initramfs 路径。

---

### G. 多重引导与虚拟化引导

**拟标题**：多重引导与虚拟化引导：GRUB chainload、Type-1 Hypervisor 启动、KVM/QEMU guest boot

**价值**：多重引导（dual-boot）、Xen/KVM 等 Type-1/Type-2 Hypervisor 的启动路径，以及 QEMU 在固件仿真（SeaBIOS/OVMF UEFI）中如何模拟 x86 启动环境，是虚拟化平台工程师和安全研究者的必备知识。Xen 作为 Type-1 Hypervisor 本身就是一个 bootloader 等效物；KVM guest 通过 QEMU + SeaBIOS/OVMF 启动，与裸机路径既相同又有差异。对虚拟机逃逸研究、固件仿真、Hypervisor 安全评估有直接价值。

**关键子话题**：GRUB chainload 与 chainloader +1 原理；GRUB multiboot/multiboot2 协议；Xen 作为 Dom0/DomU 启动拓扑；KVM guest 的 BIOS 仿真（SeaBIOS）vs UEFI 仿真（OVMF）；OVMF 在 QEMU 中的 pflash 存储；PVH 直接启动协议；virtio 设备在 guest 早期枚举。

---

### H. coreboot / LinuxBoot 开源固件

**拟标题**：coreboot 与 LinuxBoot：开源固件的启动链原理与安全含义

**价值**：coreboot 把传统 BIOS/UEFI 替换为极精简的开源实现，LinuxBoot 进一步把 Linux 内核作为固件 payload 直接运行，大幅缩减攻击面。这一方向在服务器（Google、Facebook 基础设施）、Chromebook、嵌入式安全设备中已规模部署，是供应链安全和固件逆向的重要研究对象；同时与第 02 篇（BIOS/UEFI）形成"主流商用固件 vs 开源替代"的对比视角。

**关键子话题**：coreboot 架构（romstage/ramstage/payload）；coreboot payload 选项（SeaBIOS / edk2 / GRUB / Linux）；LinuxBoot 的 u-root initramfs；Heads（开源可信启动固件）的 Measured Boot 实现；coreboot 的 CBFS（coreboot 文件系统）格式；Intel FSP（Firmware Support Package）在 coreboot 中的角色；安全影响：开源固件可审计性 vs 商用固件黑盒。

---

## 三、建议优先级

| 优先级 | 篇目 | 理由 |
|---|---|---|
| ★★★ 高 | A. Windows 启动链 | 与 Linux 并列的主流平台，逆向/安全必备，Vault 中 Windows 系列已有 38 篇，启动链是天然补充 |
| ★★★ 高 | C. Android 启动链（ABL/AVB） | 已有 35 系列 Android 逆向，启动链是前置知识，dm-verity 在第 13 篇仅提到结论 |
| ★★★ 高 | D. 休眠与恢复 | 启动链的重要变体路径，现有 13 篇完全未涉及，取证/固件/内核开发必备 |
| ★★ 中 | B. macOS/Apple Silicon 启动链 | 已有 36 系列 iOS/ObjC，启动链是自然前置；Apple Silicon 架构独特值得独立篇 |
| ★★ 中 | E. kexec / kdump | Linux 特有且实用，kdump 是生产内核崩溃分析的标准基础设施 |
| ★★ 中 | F. initramfs 深入 | 现有第 11 篇覆盖浅，排障价值极高，与 dracut/mkinitcpio 构建机制均未涉及 |
| ★ 低 | G. 多重引导与虚拟化引导 | 有价值但与 04 篇 GRUB 和可能的 Hypervisor 专章有重叠风险 |
| ★ 低 | H. coreboot/LinuxBoot | 小众但重要，适合在固件深度研究者中定位；非大众需求 |
