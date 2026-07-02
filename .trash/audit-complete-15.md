# 完备性审查报告：15 操作系统加载与启动

审查日期：2026-06-30  
审查标准：穷尽主题、不设字数上限，七维度（机制/算法/工具/边界/横向对比/历史演进/逆向实战）  
评级：✅完备 | ⚠️可补强 | ❌明显遗漏

---

## 系列总体印象

全系列 13 篇结构清晰、机制讲述深度良好、均含 Mermaid 流程图与实战命令，整体质量属于优质技术知识库水准。无篇目达到 ❌ 评级，但有 12 篇存在可识别的重要知识盲点，集中在：ARM/非 x86 平台、SMP 多核、替代方案横向对比、部分子机制深度三个维度。

---

## 逐篇审查

### 01.加电到用户态的启动全景 — ⚠️可补强

**已覆盖**：完整 5 段接力（固件→引导器→内核→init→ELF）、BIOS vs UEFI 对比表、GRUB 多级结构、boot protocol 关键字段、start_kernel() 调用树、initramfs、systemd、ELF 加载、dmesg/systemd-analyze/QEMU+GDB 示例。

**遗漏点**：

- **ARM / RISC-V 平台重置向量完全缺失**：x86 上电后 CS:IP=0xF000:FFF0 →BIOS ROM；ARM64 复位向量由 VBAR_EL3 决定、由 TF-A BL1 接管；RISC-V 的 M-mode MTVEC 初始上电路径。全景篇作为总览应有跨架构视角，目前只有 x86 视角。
- **UEFI EFI Stub 直接引导路径缺失**：现代 Linux 内置 EFI Stub（`CONFIG_EFI_STUB`），UEFI 固件可直接把 bzImage 当 PE32+ 程序执行，绕过 GRUB，这条路径在总览图中缺失。
- **PXE / iPXE 网络引导路径未提及**：企业数据中心的主流无盘引导方式，应在全景总览中有一席之地。
- **Bootloader 多样性（SYSLINUX / systemd-boot / rEFInd）**：全景图只列 GRUB，读者会误以为 GRUB 是唯一选项。

---

### 02.BIOS与UEFI固件 — ⚠️可补强

**已覆盖**：POST 流程、BIOS 物理内存布局（IVT/MBR/BIOS ROM）、INT 10h/13h/15h/16h 服务、A20 门三种方式、UEFI 四阶段（SEC/PEI/DXE/BDS）、ESP 目录结构、PE32+ 文件格式、Boot Services/Runtime Services 完整函数表、UEFI 变量（BootOrder/Boot####/PK/KEK/db/dbx）、Secure Boot shim 链、CSM。

**遗漏点**：

- **ACPI 表结构完全缺失**：UEFI/BIOS 最重要的输出之一是 ACPI 表（RSDP→RSDT/XSDT→FADT/MADT/DSDT/SSDT）。操作系统通过 ACPI 枚举 CPU 拓扑、APIC ID、电源管理能力、热插拔设备。`start_kernel()` 里 `setup_arch()` 大量依赖 ACPI，本篇对其只字未提。
- **SMM（System Management Mode）未覆盖**：SMM 是比 Ring 0 更高特权的 CPU 模式（触发 SMI），固件用它做电源管理、热节流、安全功能；也是著名固件后门通道，与 Secure Boot 侧信道攻击高度相关，本篇完全缺失。
- **UEFI Capsule Update 机制未提**：固件更新的标准路径（ESRT / `fwupd` 工具链），是固件安全关键一环。
- **UEFI Shell 实战**：`bcfg boot dump`、`map`、`load` 等实际固件调试命令未示范。

---

### 03.MBR与GPT与引导扇区 — ⚠️可补强

**已覆盖**：MBR 512 字节精确布局（446B 代码 / 4×16B 分区表 / 0x55AA）、分区表项全字段、GPT 完整结构（protective MBR / GPT header / 分区条目 / 备份副本）、GPT 分区条目字段（TypeGUID / UniqueGUID / StartLBA / EndLBA / Attributes / Name）、Bootkit 感染与防御、dd/xxd/fdisk/gdisk/Python 解析示例。

**遗漏点**：

- **VBR（Volume Boot Record）缺失**：BIOS→MBR 链下一步是 MBR 引导代码跳入 VBR（分区的第一个扇区），VBR 再引导 GRUB stage1.5 或 Windows bootmgr。这是 BIOS 引导链第二棒，本篇完全没有。
- **扩展分区与逻辑分区机制缺失**：MBR 只有 4 个主分区槽，扩展分区（Type=0x05/0x0F）+逻辑分区链（EBR 链表）是绕过限制的传统方式，不少实战系统仍在使用。
- **Advanced Format 4K 扇区对齐**：4Kn / 512e 驱动器的分区对齐要求（1MiB 对齐的来源、misaligned 分区对性能的影响），是现代分区实战中高频遇到的问题，完全缺失。
- **混合 MBR（Hybrid MBR）**：macOS/双引导场景中同时维护 MBR+GPT 分区表的特殊格式，逆向分析常见。

---

### 04.引导加载器GRUB与U-Boot — ⚠️可补强

**已覆盖**：GRUB2 三级结构（boot.img→core.img→stage2）、grub.cfg menuentry 语法、Multiboot 1/2 规范（魔数）、U-Boot SPL 链（ROM→SPL→U-Boot proper）、环境变量（bootcmd/bootargs/bootdelay/fdtfile）、DTB 格式（fdt_header/0xD00DFEED）、ARM64 内核入口约定（x0=DTB 物理地址）、安全：GRUB 密码（PBKDF2）、Secure Boot 链（shim→GRUB→kernel）、U-Boot UART 攻击面。

**遗漏点**：

- **替代 UEFI 引导器横向对比缺失**：`systemd-boot`（简洁，直接读 `/loader/entries/`）、`rEFInd`（多系统探测）、`syslinux/isolinux/pxelinux`（传统/网络/光盘引导）的适用场景、配置方式，一字未提。嵌入式场景还有 Barebox。
- **GRUB cryptodisk（加密 /boot）**：GRUB 解密 LUKS 分区后才能读内核的完整流程（`grub-mkconfig` 生成 cryptomount 命令、crypto 模块加载），是全盘加密方案的重要环节。
- **U-Boot FIT Image（Flattened Image Tree）**：现代 U-Boot 内核/DTB/initramfs 打包格式（替代独立 uImage），在 Android 设备/嵌入式 Linux 广泛使用，本篇未提。
- **GRUB 模块系统与救援模式**：`rescue>` / `grub>` 提示符区别、手动引导命令（`set root=(...)`、`linux`、`boot`）是实际救援时必需的逆向视角知识。

---

### 05.从实模式到保护模式到长模式 — ⚠️可补强

**已覆盖**：实模式 16 位段:偏移/1MB 空间、GDT/段描述符（8 字节完整字段）/段选择子（Index/TI/RPL）、Ring 0-3、长模式 EFER MSR/FS-GS base via MSR/强制分页、A20 三种方式（BIOS INT 15h/8042 KBC/Fast A20 port 0x92）、三段完整汇编伪代码、关键寄存器位表、三重故障调试表。

**遗漏点**：

- **VM86 模式（Virtual 8086）完全缺失**：VM86 是保护模式下模拟实模式 16 位代码的机制（EFLAGS.VM=1），曾用于在 DOS 保护模式环境中运行传统 TSR；现代内核虽较少使用，但在逆向/固件分析中仍可见，且是"x86 模式全集"中的一个。
- **SMM（System Management Mode）未提**：CPU 在收到 SMI 时从任意模式切入 SMM（独立地址空间、对操作系统完全不可见）；SMM 内存（SMRAM）的保护、SMBASE 寄存器、SMM 到普通模式的 RSM 指令——这是 x86 特权模式体系的重要一员。
- **CPUID 探测在模式切换前的作用**：切入长模式前需先用 CPUID 确认 `CPUID.80000001h:EDX[bit29]=1`（Long Mode 支持）；切入 64 位前探测 `CPUID.07h:ECX[LA57]` 决定是否用 5 级分页，这部分能力探测流程未展开。
- **32 位保护模式分页（PAE 之前的 PSE-36 方案）**：历史演进中 32 位下的扩展物理地址（4GB+ via PAE vs PSE-36 的区别）未提及。

---

### 06.内核解压与早期初始化 — ⚠️可补强

**已覆盖**：bzImage 三层结构、boot protocol setup 头所有关键字段、startup_32→startup_64→decompress_kernel()→startup_64（vmlinux）→start_kernel() 完整链路、KASLR choose_random_location()、start_kernel() 详细调用序列、rest_init() PID 0/1/2 职责、kernel_init do_initcalls()→initramfs→run_init_process()、安全：cmdline 注入/nokaslr/nopti/kernel lockdown。

**遗漏点**：

- **SMP 多核 AP 唤醒流程完全缺失**：BSP（Bootstrap Processor）完成 `start_kernel()` 后，其他 CPU（AP，Application Processor）通过 INIT-SIPI-SIPI 序列被唤醒、执行 `secondary_startup_64`、加入调度器，这是 SMP 系统启动的关键路径，本篇只字未提（`dmesg` 中 `Brought up N CPUs` 背后的机制）。
- **memblock 分配器（早期物理内存管理）**：在 buddy 系统初始化之前，内核使用 memblock 管理物理内存（`memblock_reserve`/`memblock_add`）；startup_64 到 mm_init() 之间的内存分配全靠它，未展开。
- **ARM64 早期启动序列**：ARM64 的 `arch/arm64/kernel/head.S`（`_text` 入口、x0=DTB、MMU 初始化顺序）与 x86 的 `startup_64` 对比，本篇只讲 x86，无 ARM64 视角。
- **initrd vs initramfs 的历史差异**：旧式 initrd（挂载为 block device）vs 现代 initramfs（cpio 解包到 rootfs）的机制区别，以及内核如何区分两者。

---

### 07.分页与页表机制 — ⚠️可补强

**已覆盖**：48 位 VA 结构（16 位符号扩展 / 4×9 位索引 / 12 位偏移）、四级层次（PML4/PDPT/PD/PT）、PTE 完整 64 位布局（P/RW/US/PWT/PCD/A/D/PS/G/PFN/NX 各位含义）、大页（2MB/1GB PS=1）、CR3 结构（PML4 基址+PCID）、TLB 硬件 walker/invlpg/G 位/PCID（Linux 4.14+）、数值走表例子、5 级分页（LA57）、安全（NX/US/W^X/SMEP/SMAP/CR0.WP/KPTI）。

**遗漏点**：

- **ARM64 分页体系完全缺失**：TTBR0_EL1（用户空间）/TTBR1_EL1（内核空间）、TCR_EL1 配置（T0SZ/T1SZ/TG0/TG1）、EL0/EL1/EL2/EL3 的独立翻译机制、ARMv8 PTE 格式（与 x86 PTE 字段名和位置差异显著），是跨架构理解的重大空白。
- **EPT（Extended Page Tables）/ 嵌套分页**：VMX 下 Guest 物理地址→Host 物理地址的第二层翻译（EPT / AMD NPT），VMCS 中 EPTP 字段、EPT Violation（VM Exit 44）与 #PF 的区别——本篇在"分页机制"主题下未提及虚拟化层。
- **IOMMU / VT-d 页表**：DMA 攻击防护的核心是 IOMMU 为设备维护独立的 I/O 页表，阻止 DMA 越界写内核内存；`intel_iommu=on` / DMAR 表完全缺失。
- **页表隔离 vs PCID 性能数据**：KPTI 开启后系统调用性能退化数据（Skylake 约 5-30%），以及 PCID 如何部分弥补，缺乏量化分析。

---

### 08.虚拟内存子系统 — ⚠️可补强

**已覆盖**：VMA 红黑树、按需调页流程、#PF（vector 14）错误码位（P/W/U/RSVD/I/PK/SGX）、do_page_fault() 伪代码、minor/major fault 对比、COW（fork() 写时复制 #PF P=1,W=1 → 复制页）、vfork()、LRU active/inactive 列表、kswapd 水位（high/low/min）/直接回收、swap entry PTE 格式（P=0 bits）、OOM killer oom_score(0-1000)/oom_score_adj、overcommit vm.overcommit_memory 0/1/2、page cache（XArray keyed by inode+page_index）、MADV_* 提示、安全（Flush+Reload / KASLR+KPTI / swap 数据泄漏 / mlock）。

**遗漏点**：

- **KSM（Kernel Samepage Merging）缺失**：内核扫描相同内容的匿名页并将其合并（KSM daemon `ksmd`），被 QEMU/KVM 大量使用以节省宿主机内存，`/sys/kernel/mm/ksm/` 调优参数，以及 KSM 带来的侧信道（rowhammer 配合 KSM 取消共享的时序泄漏），未提及。
- **zswap / zram 压缩内存路径**：现代系统（Android / 内存受限嵌入式）用 zswap 在换出到磁盘前先压缩、zram 作为内存内 swap 设备，减少磁盘 I/O，这条路径完全未覆盖。
- **NUMA 内存策略**：`numactl --membind`、`mbind()`、`set_mempolicy()` 系统调用的语义（MPOL_BIND/PREFERRED/INTERLEAVE）、`/proc/<pid>/numa_maps`，以及 NUMA 跨节点内存访问对性能的影响——本篇没有 NUMA 视角。
- **Transparent Huge Pages（THP）与 VMA 的交互**：THP 的 `khugepaged` 守护进程、`/sys/kernel/mm/transparent_hugepage/enabled`、THP 分裂（split_huge_page）时机，以及对 VMA 分割/合并的影响。
- **mmap 文件映射的脏页回写路径**：`msync(MS_ASYNC/MS_SYNC)`、pdflush/writeback 机制、`dirty_ratio`/`dirty_background_ratio`，缺失完整的文件页写回路径描述。

---

### 09.中断异常与IDT — ⚠️可补强

**已覆盖**：三类（异常/中断/软中断）、Fault/Trap/Abort 分类、IDT 256 项/16 字节门描述符/IDTR/lidt、Interrupt Gate vs Trap Gate（IF 清除差异）、门描述符完整布局、32 个 CPU 异常向量（0-31 助记符/名称/类型/错误码）、TSS（RSP0/RSP1/RSP2 和 IST1-7）、IST 用于 #DF/#MC/NMI、8259 PIC（15 IRQ / port 地址 / EOI）、APIC（LAPIC MMIO/x2APIC via MSR + IO-APIC Redirection Table + MSI/MSI-X）、安全（IDT 钩子/SMP 竞态/DPL 保护）。

**遗漏点**：

- **软中断（softirq）/ tasklet / workqueue 下半部层次缺乏深度**：中断处理分上半部（硬中断 ISR，要求极快）和下半部（延迟处理）；softirq 的 10 个固定类型（NET_TX/NET_RX/BLOCK/TASKLET 等）、tasklet 的静态优先级、workqueue 的线程化（`kworker`）——这是驱动/内核开发中最核心的概念之一，本篇仅简略提及，没有结构化展开。
- **ARM64 GIC（Generic Interrupt Controller）**：GIC-v3/v4 的分发器（Distributor）/重分发器（Redistributor）/CPU 接口（ICC_*系列 System Registers）、SPI/PPI/SGI/LPI 中断类型，与 x86 APIC 的横向对比完全缺失。
- **IRQ 亲和性（IRQ affinity）**：`/proc/irq/<n>/smp_affinity`、`irqbalance` 守护进程、NUMA 感知 IRQ 分发对网络/存储性能的影响，实战中高频调优点，未覆盖。
- **中断延迟与实时性**：`PREEMPT_RT` 补丁如何将硬中断转为 kthread 线程化中断、`cyclictest` 延迟测量工具——对嵌入式实时系统读者是关键缺失。

---

### 10.系统调用机制 — ✅完备

**覆盖评价**：本篇对 x86-64 syscall/sysret MSR 三剑客、entry_SYSCALL_64 汇编流程、swapgs/per-CPU 机制、寄存器 ABI（R10 vs RCX 的区分）、int 0x80 legacy 路径、vDSO（seqlock/AT_SYSINFO_EHDR/vvar 页）、多架构对比（ARM64/ARM32/RISC-V/MIPS/PowerPC）、逆向识别规律（syscall 指令特征/64位vs32位号对照）、seccomp BPF（KILL/ERRNO/TRAP/TRACE）、LSTAR 篡改/sys_call_table 钩子/seccomp bypass/ptrace 注入等安全分析均已详尽覆盖。

**微小可补充点**（不影响完备评级）：
- `ERESTARTSYS` / `ERESTARTNOINTR`：信号传递时系统调用的自动重启机制（`SA_RESTART`）。
- Linux audit 框架（`auditd` 通过 `audit_syscall_exit` 记录 syscall 审计日志）与 seccomp 的互补关系。

---

### 11.init与systemd启动 — ⚠️可补强

**已覆盖**：PID 0/1/2 诞生、initramfs cpio 格式/目录结构、populate_rootfs() 解包、switch_root（MS_MOVE+chroot+execv）、pivot_root 与 switch_root 的使用场景区别、SysV init /etc/inittab/runlevel/S##K## 串行启动、systemd unit 类型表、.service 文件三段结构（[Unit]/[Service]/[Install]）、After/Requires/Wants/WantedBy 语义、target→runlevel 映射、cgroups system.slice/user.slice 层次、initramfs 攻击面（Evil Maid）/UKI/dm-verity/LUKS+Measured Boot、systemd 服务沙箱完整选项表（ProtectSystem/ProtectHome/NoNewPrivileges/CapabilityBoundingSet/SystemCallFilter/PrivateTmp/PrivateNetwork/DynamicUser）。

**遗漏点**：

- **替代 init 系统横向对比缺失**：OpenRC（Gentoo/Alpine，基于 shell 脚本的并行化 init）、runit（Void Linux，三状态状态机 /etc/runit/）、s6（极简监督树）、Busybox init（嵌入式）的核心设计差异，以及为何 systemd 替代 SysV 而不是这些方案——横向比较维度完全缺失。
- **socket 激活机制深度不足**：systemd 的 socket 激活（`.socket` 单元先绑定端口，`.service` 按需拉起）、内核文件描述符传递（`SD_LISTEN_FDS_START=3`）、`sd_is_socket()` API，是 systemd 并行化的核心机制之一，本篇仅有表格一行描述，缺乏实现原理。
- **容器 PID 1 问题（zombie reaping）**：容器中 PID 1 若是业务进程（非 init），孤儿进程无人 `waitpid()` 导致僵尸堆积；`tini`/`dumb-init` 的信号透传+wait 实现，`docker run --init` 标志——本篇在结尾仅有一句提及，未展开机制。
- **microcode 更新在 initramfs 中的加载**：早期 microcode 更新（AMD/Intel）通过 initramfs 第一个 cpio 段（uncompressed）在内核启动极早期加载，是安全启动链的重要一环（Spectre/Meltdown 缓解依赖 microcode 更新），完全未提。
- **dracut vs mkinitramfs 构建工具差异**：两者生成的 initramfs 内容/结构差异，影响调试和定制。

---

### 12.ELF程序的加载与执行 — ⚠️可补强

**已覆盖**：execve(RAX=59)→do_execve()→open_exec()→search_binary_handler()→load_elf_binary() 完整调用链、三种 binfmt 处理器（binfmt_elf/binfmt_script/binfmt_misc）、ELF 头验证字段（magic/EI_CLASS/EI_DATA/e_type/e_machine/e_entry/e_phoff/e_phnum）、PT_LOAD mmap（load_bias/页对齐/prot_from_flags）、BSS 匿名零页 mmap、PT_INTERP→ld.so mmap（动态路径）、静态路径（无 PT_INTERP）、静态 PIE（ET_DYN 无 interp）、初始栈完整布局（argv/envp/execfn/AT_RANDOM/auxv/argc）、关键 auxv 条目用途表（AT_PHDR/AT_PHENT/AT_PHNUM/AT_BASE/AT_ENTRY/AT_RANDOM/AT_EXECFN/AT_PAGESZ/AT_SYSINFO_EHDR/AT_HWCAP）、start_thread() 寄存器设置、安全（ASLR/PIE 的 load_bias 随机化、AT_RANDOM→栈 canary、W^X/GNU_STACK、Full RELRO、PIE+RELRO 组合防御表、binfmt_misc 风险）。

**遗漏点**：

- **setuid/setgid 特殊 execve 处理缺失**：执行 setuid 二进制时内核会清除 `AT_SECURE=1`（通知 glibc 忽略 `LD_PRELOAD`/`LD_LIBRARY_PATH`）、重置 capabilities（permitted/effective 集变化）、清理 `pdeath_signal`。这是 Unix 权限模型中最关键的安全检查点之一，完全缺失。
- **Linux capabilities 在 execve 中的继承规则**：capabilities 三集合（permitted/inheritable/effective）在 execve 时的变换规则（`file_caps` xattr、ambient capabilities、`cap_bounding_set`），对容器/least-privilege 系统编程至关重要，未覆盖。
- **LD_PRELOAD 机制的工作原理**：LD_PRELOAD 为何在 setuid 下被禁用的内核侧依据（`AT_SECURE` 标志）、ld.so 侧检查——与本章 execve 机制关联紧密，未展开。
- **`/proc/self/exe` 和文件描述符的处置**：execve 时 `O_CLOEXEC` fd 的关闭、非 O_CLOEXEC fd 的继承、以及 `/proc/self/exe` 的更新——实战中常见的安全问题来源。

---

### 13.可信启动与启动安全 — ⚠️可补强

**已覆盖**：Intel Boot Guard（eFuse/ACM/IBB 验证）、AMD PSP、ARM TF-A BL1/BL2、Secure Boot PK→KEK→db/dbx 四级层次、PE32+ Authenticode 验证、shim（Microsoft 签名/内嵌 distro CA/验证 GRUB）、MOK（mokutil --import/sign-file）、TPM PCR 24 寄存器/PCR extend 操作/PCR0-10 分配、Secure Boot vs Measured Boot 的预防 vs 记录区别、远程证明（TPM Quote/AIK/nonce）、kernel lockdown（integrity/confidentiality 模式）、dm-verity（Merkle 树/4KB 块/根哈希/per-block 验证）、IMA（execve 前哈希/扩展到 PCR[10]/policy 评估）、EVM（security.ima/security.selinux xattr 保护）、Bootkit 类型（MBR/VBR/ESP/固件植入/NVRAM 修改）、Evil Maid + TPM Sealing（TPM2_Seal/Unseal/PCR binding）、防御清单（锁定 PK/更新 dbx/TPM+PCR LUKS/lockdown/dm-verity/IMA）。

**遗漏点**：

- **AMD SEV（Secure Encrypted Virtualization）和 Intel TDX 完全缺失**：虚拟机内存加密（SEV/SEV-SNP 的 ASID 绑定加密、TDX 的 Trust Domain Extensions）、以及 AMD SNP 的完整性保护机制，是机密计算（Confidential Computing）的核心启动安全话题，本篇未涉及。
- **UKI（Unified Kernel Image）格式缺乏深度**：本篇仅在 initramfs 防御中提名，但 UKI 的实际格式（PE 文件包裹 `kernel` + `.initrd` + `.cmdline` + `.splash` + `.os-release` 等 PE 节）、`systemd-ukify` 生成工具、UEFI 直接引导 UKI 的机制、以及 UKI 与普通 bzImage+GRUB 的签名链区别，均未展开。
- **LogoFAIL 类漏洞分析**：本篇提到 LogoFAIL 漏洞名但未分析根本原因（UEFI 图像解析器（BMP/PNG/GIF）在 DXE 阶段运行、可被恶意 ESP 上的图像触发 buffer overflow、实现 pre-OS 代码执行、绕过 Secure Boot）；类似的 PixieFail（UEFI PXE 栈）漏洞集也未提。
- **Boot Guard vs. Secure Boot 的层次区别**：Intel Boot Guard 在 ACM 硬件层面验证 IBB 代码（在 BIOS 代码运行之前），Secure Boot 是软件验证 bootloader，两者防御的是不同威胁层（固件 vs. boot chain）。本篇分别讲述但缺乏"哪一层防御哪一类攻击"的系统性对比。
- **SRTM vs DRTM（Dynamic Root of Trust for Measurement）**：AMD SKINIT / Intel TXT 实现的动态信任根（从任意状态启动测量），与静态信任根（系统上电开始的 PCR[0] 链）的区别，未覆盖。

---

## 系列统计

| 评级 | 数量 | 篇目 |
|---|---|---|
| ✅ 完备 | 1 | 10.系统调用机制 |
| ⚠️ 可补强 | 12 | 01~09, 11, 12, 13 |
| ❌ 明显遗漏 | 0 | — |

**最高优先级补充建议（全系列维度）**：

1. **ARM64 平台视角**：07（分页）、06（早期初始化 head.S）、01（全景）均缺 ARM64 对应内容，建议统一补充。
2. **SMP AP 唤醒**（06）：INIT-SIPI-SIPI 序列是 SMP 系统启动的重要机制，目前完全缺失。
3. **ACPI 表结构**（02）：固件输出的 ACPI 是操作系统硬件枚举的基础，不应在 BIOS/UEFI 篇缺席。
4. **软中断/tasklet/workqueue 下半部**（09）：驱动开发基础，目前深度明显不足。
5. **setuid execve 安全处理**（12）：Unix 权限模型核心检查点，容器/安全开发必需知识。
