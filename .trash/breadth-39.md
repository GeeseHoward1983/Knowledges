# 系列39 固件与IoT嵌入式逆向 · 选题广度审查报告

**审查日期**：2026-07-01
**现有篇数**：15 篇（01–15）
**建议新增**：11 篇

---

## 一、现有 15 篇覆盖范围确认

| # | 章节标题 | 覆盖核心 |
|---|---------|---------|
| 01 | 嵌入式逆向全景与工具链 | 全貌、工具链 Binwalk/QEMU/IDA/Ghidra |
| 02 | 固件提取：物理逻辑OTA方法 | SPI Flash/JTAG 物理读、串口/FTP 逻辑、OTA 截包 |
| 03 | 固件解包与文件系统 | binwalk、squashfs/JFFS2/YAFFS2、Firmware Mod Kit |
| 04 | ARM Cortex-M与A架构速查 | Thumb/Thumb-2、异常向量表、AAPCS |
| 05 | MIPS与RISC-V嵌入式架构速查 | MIPS 延迟槽/O32、RISC-V RV32/RV64 调用规约 |
| 06 | Bootloader逆向：U-Boot与ATF | U-Boot SPL→主阶段、ATF BL1-BL33、Secure Boot 链 |
| 07 | 嵌入式Linux内核与设备树 | 内核镜像格式、DTS/DTB、驱动加载 |
| 08 | RTOS逆向：FreeRTOS与VxWorks与ThreadX | 任务/调度/内存、符号恢复 |
| 09 | IoT通信协议逆向 | MQTT/CoAP/Zigbee/Z-Wave/BLE 报文结构与抓包 |
| 10 | 硬件调试接口：JTAG与SWD与UART | OpenOCD、J-Link、flashrom、SPI Flash 直读 |
| 11 | 固件漏洞分析 | 栈溢出/命令注入/格式化字符串（固件版） |
| 12 | TrustZone与OP-TEE安全世界逆向 | ARMv8-A TrustZone、TA/CA 逆向 |
| 13 | 固件仿真：QEMU与Firmadyne与Avatar2 | 系统/用户态仿真、自动化流水线 |
| 14 | 固件差分与补丁分析 | bindiff/bsdiff、patch diffing |
| 15 | 实战：路由器固件全流程分析 | 提取→解包→仿真→漏洞定位（完整案例） |

---

## 二、知识地图盲区分析

基于领域完整知识地图，以下重要子领域在现有 15 篇中**未被独立且充分覆盖**：

### 2.1 射频与无线协议层（严重缺失）

第 09 章仅覆盖 Zigbee/Z-Wave/BLE 的**应用层报文结构与 IoT 固件中的实现逆向**，
完全未涉及：
- **SDR（软件定义无线电）硬件采集与信号解调**：HackRF/RTL-SDR、GNU Radio、频谱分析
- **LoRa/LoRaWAN**（LPWAN 主流协议，城市级 IoT 覆盖，独有 Chirp 调制）
- **433 MHz / 915 MHz 射频遥控**（门禁、车钥匙、工控遥控）
- **NFC/RFID（HF 频段）**：ISO 14443/15693、Mifare Classic 破解与固件中 NFC 栈分析

这三项都需要 SDR 硬件基础，且方法论完全不同于软件协议逆向，足以独立 1–2 章。

### 2.2 工业控制协议专章（重要缺失）

第 09 章聚焦消费级 IoT（MQTT/CoAP/Zigbee），**工业协议完全未覆盖**：
- **Modbus RTU/TCP**：最广泛的工控协议，PLC/仪表逆向必备
- **DNP3**：电力 SCADA 专用协议
- **PROFINET / EtherNet/IP**：工厂自动化以太网
- **OPC-UA**：工业互联网统一架构，越来越多 ICS 设备支持

ICS/SCADA 固件逆向与消费级 IoT 方法论差异显著（确定性实时、冗余设计、物理安全后果），值得独立一章。

### 2.3 车载总线与汽车 ECU（完全空白）

汽车嵌入式已成固件逆向的重要分支，现有 15 篇零覆盖：
- **CAN/CAN-FD**：ECU 间通信的基础
- **UDS（ISO 14229）**：ECU 诊断协议，固件刷写/读取必经之路
- **ISO-TP（ISO 15765-2）**：CAN 上的传输层分帧协议
- **AUTOSAR 架构**：现代 ECU 软件栈基础

汽车 ECU 固件提取（通过 JTAG、引导加载漏洞、OBD-II 接口）是独立且有大量公开研究的方向。

### 2.4 UEFI/BIOS 固件逆向（完全空白）

第 06 章仅覆盖嵌入式 Bootloader（U-Boot/ATF），**UEFI 是完全不同的规范体系**：
- **PI（Platform Initialization）规范**：DXE/PEI/SEC/BDS 阶段结构
- **UEFI 模块（.efi）逆向**：PE32+ 格式、protocol/GUID 机制
- **EDK2 工具链**：UEFITool、idat（IDA UEFI 插件）、chipsec
- **SmmCore 分析**：SMM（System Management Mode）是 x86 UEFI 中的最高权限层
- **BMC/IPMI/Redfish**：服务器带外管理固件，与 BIOS 关系密切

这是一个完整的独立领域，受众包括 PC/服务器安全研究员，方法论与 ARM 嵌入式完全不同。

### 2.5 硬件侧信道与故障注入（完全空白）

第 10 章涵盖物理调试接口，但**非侵入式/半侵入式物理攻击**完全未覆盖：
- **差分功耗分析（DPA/SPA）**：通过功耗波形恢复 AES/RSA 密钥，ChipWhisperer 平台
- **电磁分析（EMA）**：非接触式侧信道
- **时钟/电压毛刺（Clock/Voltage Glitching）**：绕过安全检查、跳过固件签名验证
- **激光故障注入（EMFI）**：更精密的针对芯片内部的注入
- **工具**：ChipWhisperer、Riscure Inspector

这是突破 Secure Boot、绕过 RDP（Read-out Protection）、提取无调试接口设备固件的关键手段，且在 IoT 安全竞赛（Pwn2Own Automotive）中越来越常见。

### 2.6 eMMC/NAND 芯片级存储取证（部分覆盖但不足）

第 02 章涉及 SPI Flash 直读，但 **eMMC 是更常见的存储介质**：
- eMMC 与 SPI NOR Flash 的根本区别（eMMC 内置控制器、CRC/ECC、分区结构）
- **eMMC 直读技术**：BGA 封装焊接/脱焊、SD 转接板（eMMC-to-SD adapter）
- **NAND 芯片级取证**：坏块管理、ECC 算法还原、数据恢复
- **UFS（Universal Flash Storage）**：高端设备正在替换 eMMC
- **工具**：JTAG Dancer、RT-809H、eMMC Easy

独立成章比在 02 章中简单提及更合适，因方法论差异大（需要硬件焊接技能）。

### 2.7 固件仿真进阶：Qiling/Unicorn/HALucinator（现有仅基础）

第 13 章覆盖 QEMU/Firmadyne/Avatar2，但有几个现代工具完全未涉及：
- **Qiling Framework**：跨架构用户态仿真 + Hook API + 文件系统模拟，比 qemu-user 更适合安全分析
- **Unicorn Engine**：纯 CPU 仿真库，无操作系统语义，适合片上固件（裸机）分析
- **HALucinator**：专为裸机固件设计，通过 HAL 函数拦截绕过外设依赖问题
- **renode**：开源多节点嵌入式系统仿真，支持 Cortex-M 外设建模
- **模糊测试集成**：Jackalope、AFL++ + QEMU 用户态 fuzz 流程

13 章篇幅有限，专门一章讲"裸机固件仿真进阶（Unicorn/Qiling/HALucinator）"有独立价值。

### 2.8 安全启动链绕过技术（覆盖不足）

第 06 章提到 Secure Boot 的存在，第 12 章讲 TrustZone，但**绕过技术没有专门梳理**：
- **BootROM 漏洞**：checkm8（iPhone）、Fusée Gelée（Nintendo Switch）类型的不可修补漏洞
- **U-Boot 环境变量注入**：通过修改 NVRAM 改变启动参数
- **Secure Boot bypass via rollback**：利用版本回滚绕过签名
- **DTB 注入**：修改设备树影响内核启动参数
- **Signature verification skip via fault injection**：上文侧信道的应用场景

这需要综合前几章知识，作为一个独立的"攻击链"章节有较高价值。

### 2.9 无人机与工业 PLC 固件专章（方向性覆盖空白）

- **无人机（UAV）固件**：DJI Fly 固件加密（AES-CBC 混淆 squashfs）、MAVLink 协议逆向、ArduPilot 开源对照分析
- **PLC 固件**（Siemens S7、Rockwell Allen-Bradley）：梯形图/结构化文本与机器码对照、ST-7700 通信协议
- 两者都有专项 CTF 赛题和公开研究，方法论有独特性（如 DJI 防破解机制、PLC 确定性实时约束）

可以合并为一章"工控与无人机专项逆向"。

### 2.10 固件供应链与 SBOM 分析（防御视角新兴领域）

- **固件 SBOM（Software Bill of Materials）提取**：Trivy/Syft 扫描 rootfs、开源组件识别
- **CVE 映射**：已知漏洞组件版本自动匹配（OpenSSL 旧版、BusyBox 历史漏洞）
- **固件相似度分析**：多设备/多版本固件的相似代码检测（针对 OEM 白牌设备）
- **工具**：Firmwalker、EMBA、ShouldI、FirmSec

这是固件安全研究流程中"快速定向"阶段的标准方法，PSIRT 团队日常使用，值得独立成章。

### 2.11 BMC/IPMI 与服务器带外管理固件（完全空白）

- **BMC（Baseboard Management Controller）架构**：OpenBMC、ASPEED AST2xxx 芯片
- **IPMI 2.0 协议**：`ipmitool` 用法、历史漏洞（IPMI cipher 0 认证绕过）
- **Redfish API**：REST-based BMC 接口，越来越多服务器支持
- **BMC 固件提取与分析**：从 SPI Flash 读 BMC 固件、逆向 OpenBMC 定制层
- **与主机固件（UEFI/BIOS）的交互**：ME（Intel Management Engine）与 BMC 信任关系

服务器安全领域（数据中心、云基础设施）正越来越关注 BMC 安全，公开 CVE 频繁，且方法论与消费级 IoT 完全不同。

---

## 三、优先级排序

| 优先级 | 拟新增章节 | 理由 |
|--------|-----------|------|
| P1（强烈建议）| 16. SDR与射频信号逆向 | 无线协议逆向基础，09章完全未覆盖 |
| P1（强烈建议）| 17. 硬件侧信道与故障注入 | 绕过 Secure Boot 的关键手段，无替代覆盖 |
| P1（强烈建议）| 18. 工业控制协议逆向（Modbus/DNP3/OPC-UA） | ICS/SCADA 固件逆向独立方向，与消费IoT方法论差异大 |
| P1（强烈建议）| 19. 车载总线与汽车ECU逆向 | 汽车安全是快速增长的子领域，完全空白 |
| P2（建议）| 20. UEFI/BIOS固件逆向 | PC/服务器固件逆向独立体系，与嵌入式互补 |
| P2（建议）| 21. 安全启动链绕过技术 | 综合攻击链梳理，覆盖漏洞 |
| P2（建议）| 22. eMMC/NAND芯片级存储取证 | 02章仅提 SPI Flash，eMMC 覆盖不足 |
| P3（可选）| 23. 固件仿真进阶：Qiling与Unicorn与HALucinator | 13章是基础，裸机仿真进阶独立成章有价值 |
| P3（可选）| 24. BMC/IPMI与服务器带外管理固件 | 服务器固件安全独立方向 |
| P3（可选）| 25. 固件供应链与SBOM分析 | 防御视角新兴工具链 |
| P3（可选）| 26. 工控与无人机专项逆向 | 方向有特色，公开研究多 |

---

## 四、结论

**现有 15 篇 → 建议新增 11 篇（P1: 4篇，P2: 3篇，P3: 4篇）**

最高优先的盲区：**射频/SDR**（完全空白）、**硬件侧信道**（完全空白）、**工业控制协议**（与09章消费IoT方向根本不同）、**汽车ECU**（完全空白）。这四个方向在固件与IoT安全领域均有大量公开研究、CTF赛题和工业实践，且现有任何章节都没有有意义的交叉覆盖。
