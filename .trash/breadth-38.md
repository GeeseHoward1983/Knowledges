# 选题广度审查 · 系列 38「Windows内核与驱动逆向」

审查日期：2026-07-01
现有篇数：20 篇（01–20）
审查结论：**建议新增 9 篇**

---

## 一、现有 20 篇覆盖范围

| # | 篇名 | 已覆盖的核心领域 |
|---|------|-----------------|
| 01 | Windows内核架构与逆向全景 | ntoskrnl/HAL/Ring0/工具链 |
| 02 | 用户态到内核态的切换 | syscall/KiSystemCall64/ntdll |
| 03 | WDM驱动模型与IRP | DRIVER_OBJECT/DEVICE_OBJECT/IOCTL |
| 04 | KMDF与WDF框架 | WDF对象模型/WDFDEVICE/队列 |
| 05 | 内核对象与句柄 | Object Manager/EPROCESS/句柄表/OBJECT_HEADER |
| 06 | 内核内存管理 | 分页/非分页池/MDL/VAD/池标记 |
| 07 | WinDbg内核调试 | KD双机/转储/dt/lm/崩溃分析 |
| 08 | SSDT与系统调用表 | KiServiceTable/SSDT Hook/x64限制 |
| 09 | 内核回调机制 | 进程/线程/镜像/注册表回调（EDR侧重，非CM内部） |
| 10 | 内核Hook技术 | inline/IRP/IAT Hook/x64约束 |
| 11 | Rootkit原理与检测 | DKOM/链表摘除/交叉视图检测 |
| 12 | PatchGuard与DSE与HVCI | KPP/驱动签名/VBS-HVCI/BYOVD阻止列表 |
| 13 | 内核漏洞与利用基础 | 七类漏洞概念/Token窃取路径/SMEP-KASLR-HVCI缓解（池风水仅提及，BYOVD概念层） |
| 14 | 驱动逆向实战 | .sys逆向全流程/IRP-IOCTL分析 |
| 15 | IRQL与内核同步 | IRQL等级/自旋锁/DPC/APC基础分类/KEVENT/KMUTEX |
| 16 | 文件系统微过滤驱动 | minifilter/FltMgr/altitude/EDR文件监控 |
| 17 | 网络驱动与WFP | NDIS/WFP/callout/防火墙监控 |
| 18 | ETW与内核遥测 | ETW provider/ETW-Ti/EDR遥测/反取证 |
| 19 | PEB与TEB与KPCR | per-process/per-thread/per-CPU结构 |
| 20 | Windows安全机制与对抗全景 | PPL/VBS/HVCI/Credential Guard/ELAM/WDAC全景 |

---

## 二、知识地图缺口分析

以下主题在领域内公认重要，但现有 20 篇均未专门覆盖：

---

### 建议新增篇 A：APC 机制深度剖析（内核 APC / 用户 APC）

**缺口**：第 15 篇"IRQL与内核同步"中 APC 仅占约 30 行，作为 IRQL 等级的说明性附属内容出现，涵盖的是 KAPC/UAPC 的基本概念分类。独立的 APC 机制专章应深入覆盖：`KAPC` 结构体全字段（`KernelRoutine`/`RundownRoutine`/`NormalRoutine`）、特殊内核 APC 与普通内核 APC 的区分、`KTHREAD.ApcState` 与 `SavedApcState` 在进程附加/分离时的切换、内核线程注入（通过 `KeInsertQueueApc` 在目标线程上下文执行任意内核代码）的防御分析、`KeGuardedRegion`/`KeCriticalRegion` 如何屏蔽 APC、用户 APC 作为 Shellcode 注入路径（`QueueUserAPC`/`NtQueueApcThread`）的 EDR 检测视角、WinDbg 逆向 APC 链的命令（`dt nt!_KAPC`/`!thread`）。APC 是理解异步 I/O、线程注入、内核 Rootkit 持久化的基础机制，且与第 15 篇 IRQL 并列而非附属，值得独立成篇。

---

### 建议新增篇 B：对象管理器深度（类型系统、命名空间、ParseProcedure）

**缺口**：第 05 篇"内核对象与句柄"已覆盖 Object Manager 基本概念（`OBJECT_TYPE`/`OBJECT_HEADER`/引用计数/句柄表），但停留在"是什么"层面，未深入覆盖：`OBJECT_TYPE_INITIALIZER` 中全套操作回调（`OpenProcedure`/`CloseProcedure`/`DeleteProcedure`/`ParseProcedure`/`SecurityProcedure`/`QueryNameProcedure`）的调用时机与逆向定位、命名对象的解析链（`OBJECT_DIRECTORY` → `OBJECT_SYMBOLIC_LINK` → `ParseProcedure` 递归）、设备对象路径解析（`\Device\HarddiskVolumeX`）如何通过 Parse 链走到文件系统驱动、`PspCidTable`（全局进程/线程 CID 查找表）的结构与取证价值、`ObRegisterCallbacks` 句柄操作过滤的防御实现细节、`ObReferenceObjectByHandleWithTag` 的内核引用计数陷阱。这是理解"为什么 DKOM 摘链不够用、还需要 PspCidTable 对比"的关键，也是 EDR 防句柄降权的核心机制。

---

### 建议新增篇 C：Job 对象、Silo 与容器隔离

**缺口**：全系列对 Job/Silo 对象无任何覆盖。`EJOB`/`EJOB.SiloState` 是 Windows 容器（WSC/Hyper-V 容器）与沙箱的内核原语：Job 对象将进程纳入同一计费/限制域（CPU 时间、内存用量、句柄计数、子进程创建权限）；Server Silo 是 Windows Server Container 的核心——提供独立的进程/命名空间视图（`SILO_USER_SHARED_DATA`/沙箱注册表 hive）。对内核安全研究者的价值：容器逃逸分析（突破 Silo 命名空间隔离）、反作弊沙箱机制、Process Isolation Level/`CreateJobObject`/`NtSetInformationJobObject` 的逆向、`!job` WinDbg 命令分析 Job 层次。WSC 在 Windows 11/Azure 场景下极为普遍，值得独立成篇。

---

### 建议新增篇 D：注册表内核实现（CM 子系统）

**缺口**：第 09 篇通过 `CmRegisterCallbackEx` 讲了注册表监控回调（EDR 视角），但注册表的内核实现完全缺位。CM（Configuration Manager）是 ntoskrnl 中最复杂的 Executive 子系统之一，应覆盖：注册表 Hive 的磁盘格式（HBASE_BLOCK/HV_CELL/KEY_NODE/VALUE_NODE 结构）、CM 如何将 Hive 映射到内核虚拟内存（Hive View 与脏页写回）、`\REGISTRY` 命名空间根与 Hive 挂载点（`CmpConfigurationSrv`）、`CM_KEY_BODY` 内核对象与用户态句柄的对应关系、离线取证（从内存转储还原注册表键值）、注册表 Rootkit（在 CM 数据结构层隐藏键）与检测方式。这是恶意软件持久化分析、取证分析的必学主题。

---

### 建议新增篇 E：Hyper-V 架构与 VBS 内核（VTL0/VTL1/VMBUS）

**缺口**：第 12 篇和第 20 篇多次提到 VBS/HVCI/Credential Guard，但均从"Windows 内核被 Hypervisor 保护"的防御结果讲，未从 Hyper-V 自身架构视角深入。独立章节应覆盖：Hyper-V 架构（Type-1 Hypervisor、Root Partition/Guest Partition、SLAT/EPT/VMCS）、VTL（Virtual Trust Level）机制（VTL0=正常内核/VTL1=Secure Kernel）、VMBUS 与 VSP/VSC（设备虚拟化通信）、Secure Kernel（`securekernel.exe`/`Ium.dll`）的逆向进入方式、Isolated User Mode（IUM/TrustLet）进程（`LsaIso.exe`/`CredentialGuard.dll`）的保护边界与逆向限制、Hyper-V 调试（利用 `--debug` 启动或 KDVM 调试 SecureKernel）。这是理解现代 Windows 防御纵深的架构基础，也是分析 Credential Guard/HVCI 绕过路径的前提。

---

### 建议新增篇 F：内核池风水与高级漏洞利用技术（Pool Grooming/Spray）

**缺口**：第 13 篇仅用 2 行提及"池头 Cookie 随机化"和"池风水"概念。内核池利用是完整的独立子领域，应覆盖：Windows 10 21H1+ Segment Heap 引进内核池（`ExAllocatePool2`/`SegPagedPool`/`SegNonPagedPool`）、VS（Variable Size）vs LFH（Low Fragmentation Heap）后端在内核池中的行为、经典池风水原理（spray → free → allocate 形成可预测布局）、Windows 10/11 缓解（池随机化/Cookie/Safe Unlinking）对传统 grooming 的影响、CVE 级别的真实池溢出/UAF 利用案例（防御分析视角，如 CVE-2021-31956 NTFS 池溢出）、利用 `PipeAttribute`/`DesktopHeap` 等代理对象的现代 grooming 技术、`!pool`/`!poolused`/`!analyze` 调试技巧。这是内核漏洞分析的核心进阶内容，与第 13 篇互补但深度完全不同。

---

### 建议新增篇 G：BYOVD 攻击链与驱动供应链安全

**缺口**：第 12 篇一句话提到"BYOVD 风险→HVCI 驱动阻止列表"，第 20 篇时间线提到"BYOVD 横行促使 HVCI 强制化"，但均无深度。BYOVD（Bring Your Own Vulnerable Driver）是当前内核攻击的主流路径，值得独立成篇：BYOVD 完整攻击链（签名驱动加载绕过 DSE → 利用驱动漏洞获取内核写原语 → 禁用/移除 EDR 回调 → 持久化）、真实案例分析（BlackByte/AV Killer/Poortry/Spyboy 等驱动武器化工具链，防御分析视角）、微软驱动阻止列表（`drivers.blocklist.xml`/`WDAC 驱动阻止策略`）的工作原理与局限、LOLDrivers 数据库的研究意义、驱动供应链安全（EV 证书盗用/交叉签名遗留证书问题）、蓝队检测 BYOVD 的方法（ETW 加载事件/签名者信息比对/驱动哈希黑名单）。这是当前 APT 和勒索软件最常用的内核攻击手法，具有高度现实价值。

---

### 建议新增篇 H：GDI / Win32k 内核攻击面

**缺口**：全系列无任何 Win32k 覆盖。`win32k.sys`/`win32kbase.sys`/`win32kfull.sys` 是 Windows GUI 子系统的内核部分，历史上是内核漏洞最密集的攻击面（CVE 数量长期居于首位）。独立章节应覆盖：Win32k 架构（从 NT 内核角度看 Win32k 是 Shadow SSDT 的扩展服务表、W32pServiceTable）、GDI 对象（`BASEOBJECT`/`GDI_HANDLE_TABLE`/`pobj` 指针数组）与 User 对象（`tagWND`/`tagMENU`/`tagACCEL`）的内核表示、Win32k 历史高危漏洞类型（User 对象池 UAF、GDI 位图读写原语/CVE-2015-0003、SetWindowLong UAF 类）、Win32k 攻击面收缩（Win10 RS1 引入 Win32k 系统调用过滤：AppContainer/沙箱进程可禁用 Win32k 调用）、`Shadow SSDT` 的逆向定位与 Hook 检测。对分析提权漏洞、浏览器沙箱逃逸（IE/Chrome 沙箱逃逸必经 Win32k）不可或缺。

---

### 建议新增篇 I：CI.dll、代码完整性策略与 WDAC 深度

**缺口**：第 12 篇从 HVCI 角度提到代码完整性，第 20 篇在 WDAC 条目下有几行，但 `CI.dll` 的内部机制与 WDAC 策略引擎均未专门覆盖。独立章节应覆盖：`CI.dll` 在驱动加载流程中的位置（`SeValidateImageHeader`/`CiValidateImageHeader` 调用链）、Authenticode 签名验证的内核实现（PE 证书目录→哈希验证→证书链构建→TSP 校验）、Catalog 签名（`CatRoot`）vs 内嵌签名的处理差异、WDAC（Windows Defender Application Control）策略的编译格式（Binary Policy/`P7B`）与内核加载验证流程（`SeILSigningPolicy`）、Supplemental Policy 与 AppID Tagging Policy 机制、`CIPolicy` WMI 接口与 PowerShell 策略管理、Managed Installer（SCCM 可信安装）标记的内核实现（EAs + CI 信任链）、蓝队使用 WDAC 限制 LOLBins/BYOVD。这是 EDR 开发者和蓝队工程师理解"为什么已签名却不能运行"的必读内容。

---

### 建议新增篇 J：Windows 内核 Fuzzing（WDF 驱动 / IOCTL 接口）

**缺口**：全系列无 Fuzzing 覆盖。内核 Fuzzing 是驱动安全研究的核心方法论，应覆盖：IOCTL Fuzzing 的基本思路（枚举设备名/IOCTL 功能码/输入格式盲测）、WinAFL 内核模式 Fuzzing 配置、kAFL（基于 Intel PT 的内核 Fuzzer）原理与 Windows 驱动 Fuzzing 实战、`KASAN`（Kernel Address Sanitizer，Windows 21H2+ 支持）与 Driver Verifier 的 Fuzzing 辅助（Enhanced I/O Verification/Pool Tracking/Strict Fault Injection）、`syzkaller` 对 Windows 的支持现状、从 Fuzzing 崩溃转储到 PoC 的分析流程（`!analyze -v`/Crashdump 关联）、Fuzzing 发现的真实驱动漏洞案例（防御分析视角）。这是漏洞挖掘研究者和安全工程师的必备方法论章节。

---

## 三、决策说明（排除的候选）

以下方向经审查后**不建议独立新增**，原因如下：

| 候选方向 | 已有覆盖 / 排除原因 |
|---------|-------------------|
| KDNET/KDGDB 远程调试搭建 | 第 07 篇"WinDbg内核调试"的调试基础已覆盖，KDNET 是配置细节，不足以独立成篇 |
| APC 机制（基础） | 第 15 篇有基本分类，**但**本审查建议 A 篇是"深度专章"，两者不冲突 |
| Store 驱动/DriverKit | 本系列定位为 Windows 传统内核 + 驱动逆向，Store 驱动属于边缘场景，优先级低 |
| 内核调度器深度 | 与 IRQL/同步（第 15 篇）高度重叠，独立拆分的边际价值有限 |
| NUMA/内存管理进阶 | 第 06 篇内存管理已有中等深度，进一步细化与本系列逆向主线偏离 |

---

## 四、建议新增篇汇总

| 拟编号 | 拟标题 | 一句话价值 |
|--------|--------|-----------|
| 21 | APC 机制深度剖析 | 覆盖内核/用户 APC 完整结构与线程注入分析，是第 15 篇 APC 三十行的专章延伸 |
| 22 | 对象管理器深度（类型系统与命名空间） | 揭开 ParseProcedure/PspCidTable/ObRegisterCallbacks 等 05 篇未触及的进阶层 |
| 23 | Job 对象、Silo 与 Windows 容器隔离 | 容器/沙箱逃逸研究的内核原语基础，全系列空白 |
| 24 | 注册表内核实现（CM 子系统） | Hive 磁盘格式、CM 内存结构、离线取证与注册表 Rootkit 检测，全系列空白 |
| 25 | Hyper-V 架构与 VBS 内核 | VTL0/VTL1/SecureKernel/VMBUS 架构，是 12+20 篇"防御结果"的架构底层 |
| 26 | 内核池风水与高级利用技术 | Segment Heap 内核池、现代 grooming 技术与 CVE 案例，13 篇概念层的进阶专章 |
| 27 | BYOVD 攻击链与驱动供应链安全 | 当前 APT/勒索软件主流内核攻击路径的完整分析，12 篇一句话的专章延伸 |
| 28 | GDI/Win32k 内核攻击面 | 历史漏洞最密集的内核子系统，浏览器沙箱逃逸与提权研究不可或缺，全系列空白 |
| 29 | CI.dll 与代码完整性策略（WDAC） | Authenticode 内核验证链、WDAC 策略引擎，EDR/蓝队工程师的必读内容 |
| 30 | Windows 内核 Fuzzing | IOCTL Fuzzing/kAFL/Driver Verifier 辅助，漏洞挖掘方法论，全系列空白 |
