# 质量审查报告 review-07

**审查时间**：2026-06-30  
**审查目录**：  
- `38.Windows内核与驱动逆向`（31 篇）  
- `39.固件与IoT嵌入式逆向`（27 篇）  
- `32.hooks`（9 篇）  
**审查方法**：每篇 Read 前 ~150 行 + 扫 `## ` 标题，快扫判断标题切题度与实质技术内容。

---

## 问题篇清单

**无问题篇。全部 67 篇均通过审查。**

---

## 各目录统计

| 目录 | 总篇数 | 问题篇数 | 结论 |
|------|--------|----------|------|
| `38.Windows内核与驱动逆向` | 31 | 0 | 全部通过 |
| `39.固件与IoT嵌入式逆向` | 27 | 0 | 全部通过 |
| `32.hooks` | 9 | 0 | 全部通过 |
| **合计** | **67** | **0** | **全部通过** |

---

## 审查备注

### 38.Windows内核与驱动逆向（31 篇）

覆盖话题完整、技术深度足够：

- 内核架构全景（Ring0/3、混合内核、HAL）
- 用户态→内核态切换（ntdll→syscall→KiSystemCall64→SSDT）
- WDM/KMDF/WDF 驱动模型与 IRP/IOCTL 流程
- 内核对象/句柄、内存管理（MDL/分页池/VAD）
- WinDbg 双机调试、符号、崩溃转储
- SSDT、内核回调（进程/线程/镜像/注册表/句柄）
- Inline Hook、IRP Hook、IAT/IDT Hook
- Rootkit（DKOM 摘链）、DKOM 交叉视图检测
- PatchGuard/DSE/HVCI 三层防线
- 内核漏洞基础（Token 窃取/SMEP/KASLR/kCFG）
- IRQL/自旋锁/DPC/APC 同步机制
- minifilter/FltMgr、NDIS/WFP、ETW 遥测
- PEB/TEB/KPCR、APC 深度（Early Bird 注入）
- 对象管理器深度（OBJECT_HEADER/PspCidTable）
- Job/Silo 容器隔离、注册表 CM 子系统（regf）
- Hyper-V/VBS（VTL/Secure Kernel/EPT）
- 内核池风水与 Segment Heap 利用
- BYOVD 攻击链、Win32k/GDI 攻击面
- CI.dll/WDAC 代码完整性、内核 Fuzzing（kAFL/syzkaller）

格式说明：`21.APC机制深度.md` frontmatter 仅含 tags，无 title/series 字段，属格式差异，不影响内容质量。

### 39.固件与IoT嵌入式逆向（27 篇）

覆盖话题完整、技术深度足够：

- 固件提取（SPI Flash/JTAG/UART/OTA 三路径）
- binwalk/squashfs/JFFS2/UBIFS 解包
- ARM Cortex-M/A（Thumb-2/向量表/AAPCS）、MIPS（延迟槽/O32 ABI）、RISC-V
- Bootloader 逆向（U-Boot SPL→主阶段、ATF BL1-BL33、Secure Boot）
- 嵌入式 Linux 内核（zImage/DTB/设备树）、RTOS（FreeRTOS/VxWorks/ThreadX TCB/QCB）
- IoT 协议（MQTT/CoAP/Zigbee/Z-Wave/BLE）
- JTAG/SWD/UART TAP 状态机、OpenOCD
- 固件漏洞（栈溢出/命令注入/格式化字符串）
- TrustZone/OP-TEE（ARMv8-A 双世界/TA/CA 逆向）
- QEMU/Firmadyne/Avatar2 仿真三路径
- BinDiff/Diaphora/radiff2 补丁分析
- 路由器固件全流程 7 步实战
- SDR（GNU Radio/URH/IQ/调制解调）、侧信道/故障注入（DPA/CPA/ChipWhisperer）
- 工控协议（Modbus/DNP3/OPC-UA/S7/Purdue 模型）
- 车载总线（CAN/LIN/FlexRay/UDS/OBD-II）
- UEFI/BIOS（PI 阶段/FV/FFS/efiXplorer）、安全启动绕过（BootROM/eFuse/TOCTOU）
- eMMC/NAND 芯片级取证（chip-off/ISP/ECC/OOB/FTL/RPMB）
- Qiling/HALucinator/Fuzzware 仿真进阶
- BMC/IPMI（ASPEED AST2xxx/Redfish/OpenBMC）
- SBOM/SPDX/CycloneDX/EMBA 供应链分析
- 工控/无人机专项（PLC/S7 MC7/DJI DUML/MAVLink）

### 32.hooks（9 篇）

覆盖 6 维 Hook 技术，全部完整：

- React 18/19 Hooks 内部机制（Fiber/workInProgress 链表）、Vue 3 Composition API
- Windows API Hook（SetWindowsHookEx 14 种钩子、IAT/Inline Hook/VEH/DLL 注入）
- Linux LD_PRELOAD/GOT-PLT/ptrace/eBPF+CO-RE+BTF
- 语言运行时 Hook（Python monkey patching/sys.settrace/import hook、Node.js Module._load/async_hooks）
- Qt 3/4/5/6 全版本 Hook API 变迁对照
- Claude Code Hooks（PreToolUse/PostToolUse/Stop/SessionStart）
- Git Hooks（客户端/服务端全集、Husky/lefthook 对比）
- 13 类 Hook 技术统一对照表与选型决策树

---

*报告由自动快扫生成，审查员：Claude Sonnet 4.6*
