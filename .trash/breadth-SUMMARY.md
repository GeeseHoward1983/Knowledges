# 第三档选题广度审查汇总 · 候选新增篇目

> 审查维度：站在各领域**完整知识地图**高度，找「现有篇之外、值得独立成篇的新主题」（区别于 SUMMARY 的"篇内是否讲全"）。
> 各系列详报见 `breadth-{编号}.md`。**这是候选池，非承诺全建——供挑选**。

## 总览

| 系列 | 现有 | 建议新增 | 领域最痛缺口（P1） |
| --- | --- | --- | --- |
| 23 运行时与ABI | 14 | 8 | Windows SEH/unwind、协程ABI、sanitizer运行时、栈保护(CET) |
| 24 内存管理与堆 | 13 | 10 | Windows堆(LFH/Segment)、Scudo、House of利用手法体系 |
| 15 操作系统加载启动 | 13 | 8 | Windows启动链、macOS启动、Android AVB启动 |
| 38 Windows内核驱动 | 20 | 10 | BYOVD、内核池风水、Win32k攻击面、Hyper-V/VBS、内核Fuzzing |
| 39 固件与IoT | 15 | 11 | SDR射频、故障注入、工业协议(Modbus/DNP3)、车载(CAN/UDS) |
| 45 反编译器引擎 | 15 | 10 | DL二进制相似性、MBA反混淆、污点分析、DWARF/PDB、BinaryNinja |
| 40 WASM与虚拟化壳 | 13 | 9 | Themida、传统壳、脱壳实战(OEP/IAT)、OLLVM、反调试全景 |
| 46 构建系统 | 13 | 9 | MSBuild/.NET、交叉编译、Buck2、Nix、monorepo工具 |
| 72 抓包与协议 | 13 | 9 | HTTP/3 QUIC、gRPC/Protobuf、JA4/JARM/HASSH |
| **合计** | **129** | **~87** | — |

---

## 23 运行时与ABI（+8）
A. Windows x64 SEH 与 Unwind ABI｜B. C++ 协程 ABI 与协程帧布局｜C. Sanitizer 运行时(ASan/UBSan/TSan)｜D. std::function/类型擦除的 ABI｜E. LTO/内联对 ABI 边界的影响｜F. 栈保护运行时(Canary/CET Shadow Stack/SafeStack)｜G. C++20 Modules 对 ABI 的影响｜H. Rust/Swift 与 C++ ABI 互操作

## 24 内存管理与堆（+10）
Windows堆(NT Heap/LFH/Segment Heap)｜Scudo分配器｜House of利用手法体系｜GC算法原理(标记清除/分代/并发)｜硬化分配器(hardened_malloc/OpenBSD)｜NUMA内存架构｜大页与THP｜自定义内存池/对象池｜内存压缩(zram/zswap)｜内存屏障与分配器并发

## 15 操作系统加载启动（+8）
Windows启动链(bootmgr/winload)｜macOS启动(iBoot/boot.efi/kernelcache)｜Android启动(ABL/vbmeta/AVB2.0/dm-verity)｜休眠与恢复(S3/S4/hiberfil)｜kexec与kdump｜initramfs深入(dracut)｜多重引导与虚拟化引导｜coreboot与LinuxBoot

## 38 Windows内核驱动（+10）
APC机制深度｜对象管理器深度(类型系统/命名空间)｜Job对象/Silo/容器隔离｜注册表内核实现(CM)｜Hyper-V架构与VBS(VTL/SecureKernel)｜内核池风水与高级利用｜BYOVD攻击链与驱动供应链｜GDI/Win32k内核攻击面｜CI.dll与代码完整性(WDAC)｜内核Fuzzing(IOCTL/kAFL)

## 39 固件与IoT（+11）
SDR与射频信号逆向｜硬件侧信道与故障注入(ChipWhisperer)｜工业控制协议(Modbus/DNP3/OPC-UA)｜车载总线与ECU(CAN/UDS/ISO-TP)｜UEFI/BIOS固件逆向｜安全启动链绕过｜eMMC/NAND芯片级取证｜固件仿真进阶(Qiling/HALucinator)｜BMC/IPMI带外管理固件｜固件供应链与SBOM｜工控与无人机专项

## 45 反编译器引擎（+10）
反汇编对抗技术与检测｜Binary Ninja架构与BNIL｜MBA混淆原理与自动化反混淆｜神经反编译(Seq2Seq到LLM)｜深度学习二进制相似性(Asm2Vec/jTrans)｜污点分析引擎实现｜调试信息解析(DWARF/PDB/BTF)｜变量恢复与栈帧分析｜SMT约束求解器系统应用｜反编译正确性验证

## 40 WASM与虚拟化壳（+9）
Themida/WinLicense实战｜传统壳(Enigma/Obsidium/Armadillo)｜脱壳实战(OEP/Dump/IAT重建)｜代码混淆对抗(OLLVM/Tigress)｜反调试与反VM全景｜Unicorn/Qiling模拟脱壳｜.NET混淆与VM壳(ConfuserEx)｜WASI与组件模型｜Java/Android DEX加固壳

## 46 构建系统（+9，另3篇可并入现有）
Buck2与元编程构建语言｜Nix/Guix可复现构建｜MSBuild与.NET构建｜Monorepo工具(Nx/Turborepo/Pants)｜交叉编译专章｜容器化构建(BuildKit/OCI)｜CI/CD与构建集成｜Bzlmod迁移(WORKSPACE→MODULE)｜增量链接器(mold/lld/LTO)

## 72 抓包与协议（+9）
HTTP/3与QUIC抓包解密｜gRPC/Protobuf流量分析｜TLS指纹进阶(JA4/JARM/HASSH)｜IoT消息协议(MQTT/CoAP/AMQP)｜eBPF/XDP高性能抓包｜网络取证(流重组/文件提取)｜VoIP(SIP/RTP)｜工业协议(Modbus/S7)｜蓝牙BLE与USB抓包

---

## 规模参照
- **全部候选 ~87 篇** ≈ 再建大半个第三档（第三档共 129 篇）。
- **各系列 P1 合计 ~30 篇** ≈ 补最痛缺口。
- 建议按系列分批建，每批走完整流水线（总览锚定→并发实现→审查→验收）。
