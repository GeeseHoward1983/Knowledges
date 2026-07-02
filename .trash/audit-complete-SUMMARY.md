# 第三档 9 系列完备性审查汇总

> 审查标准：「穷尽主题、不设字数上限」。查的是**内容有无遗漏/一笔带过**，非字数格式（那些已全过）。
> 各系列详细报告见 `audit-complete-{编号}.md`。

## 总览

| 系列 | 正篇 | ✅完备 | ⚠️可补强 | ❌明显遗漏 |
| --- | --- | --- | --- | --- |
| 23 运行时与ABI | 14 | 5 | 8 | **1** |
| 24 内存管理与堆 | 13 | 3 | 10 | 0 |
| 15 操作系统加载启动 | 13 | 1 | 12 | 0 |
| 38 Windows内核驱动 | 20 | 4 | 16 | 0 |
| 39 固件与IoT | 15 | ~4 | ~11 | 0 |
| 45 反编译器引擎 | 15 | 2 | 13 | 0 |
| 40 WASM与虚拟化壳 | 13 | 7 | 6 | 0 |
| 46 构建系统 | 13 | 7 | 6 | 0 |
| 72 抓包与协议 | 13 | 1 | 11 | **1** |
| **合计** | **129** | **~34** | **~93** | **2** |

**总体判断**：技术准确、结构完整、无空洞篇（唯 2 篇❌为主题范围缺角）。约 34 篇已达完备；约 93 篇有「值得补但非致命」的遗漏；2 篇有明显主题缺角。

---

## P0 · 明显遗漏（❌，强烈建议补）

- **23/05 Itanium name mangling** — 缺**构造/析构函数变体 C1/C2/C3、D0/D1/D2**。这是逆向 C++ 符号表最高频知识点，且第 07 章 vtable 双槽（D1/D0 thunk）依赖它；运算符 mangling 速查表（`pl`/`eq`/`ls`…）也未收。
- **72/07 WebSocket与HTTP2** — 开篇对比表已列 **HTTP/3(QUIC)** 但全篇零实质内容（无 QUIC 帧/QPACK/Wireshark 解密）。

---

## P1 · 系统性高价值缺口（跨多系列的共性遗漏）

1. **ARM64/AArch64 视角普遍缺失**（移动端/现代设备逆向核心）：
   - 23/12 调用规约缺 AAPCS64；23/13 TLS 缺 `TPIDR_EL0`；23/11 缺 ARM EHABI（`.ARM.exidx`）
   - 15/01、15/07 缺 ARM64 启动/分页（TTBR/TCR/EL 分级）
   - 38/01、38/02、38/12 缺 ARM64（SVC、PAC/BTI、x16）
2. **跨平台补充**：23/02 缺 macOS dyld 启动；23/13 缺 **Windows TLS（`.tls`节/TLS回调—恶意软件常用）**；23/14 缺 Bionic。
3. **关键算法未分步展开**：45/02 Lengauer-Tarjan 支配树；45/09 **MBA 混合布尔算术混淆**（VMProtect 3.x 核心）；45/12 深度学习二进制相似性（Asm2Vec/jTrans）；24/09 **realloc 三路径**整体空白。
4. **重要机制细节**：24/05 `heap_info` 多堆串联；24/12 SLUB 三级获取/归还；38/15 ERESOURCE/Push Lock；40/12 VMProtect 3.x Handler 表指针 XOR 加密。

---

## P2 · 锦上添花（各系列剩余遗漏）

多为：新工具补充（FACT/HALucinator/cargo-nextest/CMakePresets/Bzlmod/HASSH/JARM…）、更多变体（其他 RTOS/init 系统/分配器）、工业协议（Modbus/OPC-UA）、历史演进等。详见各系列 `audit-complete-XX.md`。

---

## 各系列最值得补的点（浓缩）

- **23**：C1/C2/D0/D1 变体(❌)、ARM64 AAPCS64、Windows TLS 回调、ARM EHABI
- **24**：realloc 三路径、heap_info、SLUB 三级、tcmalloc decommit vs jemalloc decay、Scudo
- **15**：ARM64 启动/分页、ACPI 表(RSDP→MADT/FADT)、SMP AP 唤醒(SIPI)、setuid execve、SEV/TDX
- **38**：ARM64(SVC/PAC/BTI)、WOW64 PEB32/TEB32、UEFI Rootkit(BlackLotus)、CET/Shadow Stack、PPL
- **39**：工业协议(Modbus/PROFINET/OPC-UA)、厂商 Bootloader(Qualcomm LK/MTK)、RISC-V 特权 ISA、故障注入(Chipwhisperer)
- **45**：MBA 混淆(❌级重要)、支配树算法、DL 二进制相似性、过程间数据流/IFDS、ESIL/REIL
- **40**：VMProtect 版本变迁、NoVmp/vmprofiler、Handler 表 XOR、angr CFGFast 局限
- **46**：CMakePresets、Bzlmod/Skyframe、`$(eval)/$(call)`、cargo-nextest、动态执行
- **72**：HTTP/3 QUIC(❌)、Protobuf 逆向(`--decode_raw`)、Lua dissector 实战、HASSH/JARM、Flutter 代理盲区

---

## 建议

- **P0（2 篇）**：应补，主题缺角。
- **P1（系统性缺口）**：高价值，尤其 ARM64 视角——移动端逆向绕不开。
- **P2**：按兴趣/需要选补，不影响主体完整性。
