---
title: "详解PE文件(十七)：TLS表"
series: PE文件详解
order: 17
source: https://blog.csdn.net/geesehoward20000/article/details/154744471
author: geesehoward20000（阿捏利）
published: 2025-12-02
collected: 2026-06-26
aliases:
  - TLS表
  - IMAGE_TLS_DIRECTORY
  - 线程局部存储
  - PE(十七)
summary: TLS（线程局部存储）表是 PE 数据目录第 10 项，核心结构 IMAGE_TLS_DIRECTORY 描述了 TLS 模板数据范围与回调函数数组；TLS 回调在进程/线程入口点之前执行，是逆向分析的高价值点。
tags:
  - PE文件
  - Windows
  - 逆向工程
  - 二进制文件格式
  - 数据目录
---
# 详解PE文件(十七)：TLS表

> 来源：[CSDN 原文](https://blog.csdn.net/geesehoward20000/article/details/154744471)　｜　发布：2025-12-02　｜　合集《PE文件详解》第 17/26 篇

> [!tip] 一句话核心
> TLS（Thread Local Storage，线程局部存储）表位于 [[07-详解PE文件(七)：数据目录|数据目录]] 第 10 项（索引 9），核心结构 `IMAGE_TLS_DIRECTORY` 描述了线程私有数据的模板区间与初始化回调链。**最关键的设计在于：TLS 回调函数在进程入口点 `WinMain`/`DllMain` 之前就会被系统调用**——这一特性既是多线程编程的利器，也是逆向分析必须关注的执行"前门"。

> [!info] 速查
> - **数据目录索引**：9（第 10 项，从 0 起算）
> - **结构体**：`IMAGE_TLS_DIRECTORY32` / `IMAGE_TLS_DIRECTORY64`
> - **大小**：32 位版 24 字节 / 64 位版 40 字节
> - **关键字段**：`StartAddressOfRawData`、`EndAddressOfRawData`、`AddressOfIndex`、`AddressOfCallBacks`
> - **位置来源**：[[06-详解PE文件(六)：可选头|可选头]] → [[07-详解PE文件(七)：数据目录|数据目录]] → 第 10 项 RVA/Size → TLS 目录节区
> - **C/C++ 声明**：`__declspec(thread)` 关键字触发编译器自动生成 TLS 数据节

TLS 是 Windows 为**多线程**场景设计的一套机制：让同一份代码里每个线程拥有各自独立的变量副本，互不干扰。PE 文件通过 TLS 表（TLS Directory）向操作系统描述这些变量的初始值、大小以及在线程生命周期各阶段需要执行的回调钩子。

理解 TLS 表，既是掌握多线程 PE 加载机制的关键，也是逆向分析中**不可跳过**的检查点——恶意软件常借助 TLS 回调在调试器"还没来得及停"的时间窗里执行反调试代码。

## TLS 表在 PE 文件中的位置

TLS 表通过 [[07-详解PE文件(七)：数据目录|数据目录]] 索引 9 定位，完整路径如下：

```text
PE 文件布局（简化）
┌──────────────────────────────────┐
│  DOS 头 (IMAGE_DOS_HEADER)        │  → e_lfanew 指向 PE 头
├──────────────────────────────────┤
│  DOS Stub                         │
├──────────────────────────────────┤
│  PE 签名 "PE\0\0"                 │
├──────────────────────────────────┤
│  COFF 文件头                      │
├──────────────────────────────────┤
│  可选头 (Optional Header)         │
│  ├─ ... (标准字段)                │
│  └─ DataDirectory[16]             │
│       ├─ [0]  导出表              │
│       ├─ [1]  导入表              │
│       ├─ ...                      │
│       └─ [9]  TLS 表  ◄──────────┼─ 本篇主角，RVA 指向下方节区
├──────────────────────────────────┤
│  节表 (Section Table)             │
├──────────────────────────────────┤
│  .tls 节（或嵌入其他节）           │  ← IMAGE_TLS_DIRECTORY 结构体所在
│  ├─ IMAGE_TLS_DIRECTORY           │
│  ├─ TLS 模板数据（Raw Data）      │
│  └─ TLS 回调函数数组              │
└──────────────────────────────────┘
```

数据目录共 16 项，TLS 在其中的排位：

| 索引 | 名称 | 备注 |
|------|------|------|
| 0 | 导出表 | [[09-详解PE文件(九)：导出表]] |
| 1 | 导入表 | [[08-详解PE文件(八)：导入表]] |
| 2 | 资源表 | [[10-详解PE文件(十)：资源表]] |
| 3 | 异常表 | [[11-详解PE文件(十一)：异常表]] |
| 4 | 证书表 | [[12-详解PE文件(十二)：证书表]] |
| 5 | 基址重定位表 | [[13-详解PE文件(十三)：基址重定位表]] |
| 6 | 调试数据 | [[14-详解PE文件(十四)：调试数据]] |
| 7 | 版权信息 | [[15-详解PE文件(十五)：版权信息]] |
| 8 | 全局指针 | [[16-详解PE文件(十六)：全局指针]] |
| **9** | **TLS 表** ⭐ | **本篇** |
| 10 | 加载配置表 | [[18-详解PE文件(十八)：加载配置表]] |
| 11 | 绑定导入表 | [[19-详解PE文件(十九)：绑定配置表]] |
| 12 | 导入地址表 | [[20-详解PE文件(二十)：导入地址表]] |
| 13 | 延迟导入表 | [[21-详解PE文件(二十一)：延迟导入表]] |
| 14 | CLR 运行时头 | [[22-详解PE文件(二十二)：CLR运行时头]] |
| 15 | 保留 | [[23-详解PE文件(二十三)：保留]] |

## TLS 表的核心数据结构

`IMAGE_TLS_DIRECTORY` 有 32 位与 64 位两个版本，字段语义相同，仅地址类型宽度不同。

### 32 位版本（IMAGE_TLS_DIRECTORY32）

```c
typedef struct _IMAGE_TLS_DIRECTORY32 {
    DWORD   StartAddressOfRawData;     // TLS原始数据起始地址(RVA)
    DWORD   EndAddressOfRawData;       // TLS原始数据结束地址(RVA)
    DWORD   AddressOfIndex;            // 指向TLS索引值的地址(RVA)
    DWORD   AddressOfCallBacks;        // TLS回调函数数组的地址(RVA)
    DWORD   SizeOfZeroFill;            // 初始化为0的数据大小
    DWORD   Characteristics;           // 特性标志位(保留，通常为0)
} IMAGE_TLS_DIRECTORY32;
```

### 64 位版本（IMAGE_TLS_DIRECTORY64）

```c
typedef struct _IMAGE_TLS_DIRECTORY64 {
    ULONGLONG   StartAddressOfRawData; // TLS原始数据起始地址(RVA)
    ULONGLONG   EndAddressOfRawData;   // TLS原始数据结束地址(RVA)
    ULONGLONG   AddressOfIndex;        // 指向TLS索引值的地址(RVA)
    ULONGLONG   AddressOfCallBacks;    // TLS回调函数数组的地址(RVA)
    DWORD       SizeOfZeroFill;        // 初始化为0的数据大小
    DWORD       Characteristics;       // 特性标志位(保留，通常为0)
} IMAGE_TLS_DIRECTORY64;
```

### 字段偏移布局对比

```text
IMAGE_TLS_DIRECTORY32（24 字节）
偏移    字段                    大小
0x00    StartAddressOfRawData   4
0x04    EndAddressOfRawData     4
0x08    AddressOfIndex          4
0x0C    AddressOfCallBacks      4
0x10    SizeOfZeroFill          4
0x14    Characteristics         4

IMAGE_TLS_DIRECTORY64（40 字节）
偏移    字段                    大小
0x00    StartAddressOfRawData   8
0x08    EndAddressOfRawData     8
0x10    AddressOfIndex          8
0x18    AddressOfCallBacks      8
0x20    SizeOfZeroFill          4
0x24    Characteristics         4
```

| 字段 | 32 位大小 | 64 位大小 | 功能简述 |
|------|-----------|-----------|----------|
| `StartAddressOfRawData` | 4 | 8 | TLS 模板数据起始 RVA |
| `EndAddressOfRawData` | 4 | 8 | TLS 模板数据结束 RVA |
| `AddressOfIndex` | 4 | 8 | 指向 TLS 索引变量的 RVA |
| `AddressOfCallBacks` | 4 | 8 | 指向回调函数指针数组的 RVA |
| `SizeOfZeroFill` | 4 | 4 | 额外零填充字节数 |
| `Characteristics` | 4 | 4 | 保留，通常为 0 |

## 各字段详解

### StartAddressOfRawData 与 EndAddressOfRawData

这两个字段共同划定了 **TLS 模板数据**在内存中的区间——每当新线程创建时，系统会将这段数据**逐字节复制**一份给该线程，作为其线程私有的 TLS 存储起点。

```text
PE 文件节区（.tls）               线程 A 的 TLS 块        线程 B 的 TLS 块
┌────────────────────┐            ┌─────────────────┐     ┌─────────────────┐
│ TLS 模板数据        │──复制────▶│ 线程 A 的副本    │     │ 线程 B 的副本    │
│ [Start ... End)    │      └───▶│ (独立，互不干扰) │     │ (独立，互不干扰) │
│                    │            └─────────────────┘     └─────────────────┘
│ + 零填充区         │
└────────────────────┘
```

- 区间为**左闭右开** `[Start, End)`，长度 = `End - Start`。
- 若模块没有 TLS 数据，两者均为 0。

### AddressOfIndex

指向一个 `DWORD`（PE32+为 64 位指针，但该 DWORD 值本身仍是 32 位）变量，由 Windows 在加载时写入**该模块的 TLS 索引**。

- Windows 内部维护一个全局 TLS 槽位表（最多 64 个"标准槽"，还有扩展区）。
- 加载器为当前模块分配一个唯一的槽位编号，写到 `AddressOfIndex` 指向的内存处。
- 进程中的任何线程可通过 `TlsGetValue(tlsIndex)` / `TlsSetValue(tlsIndex, ...)` 访问自己的 TLS 数据。
- Windows 内部路径则直接走 **TEB**（线程环境块）的 `TlsSlots` 数组，免去函数调用开销。

### AddressOfCallBacks

指向一个**以 NULL 结尾的函数指针数组**，数组里每个元素都是一个 TLS 回调的地址：

```text
AddressOfCallBacks
       │
       ▼
  ┌──────────┬──────────┬──────────┬──────────┐
  │Callback1 │Callback2 │   ...    │  NULL    │
  └──────────┴──────────┴──────────┴──────────┘
       │           │                    ▲ 数组终止标记
       ▼           ▼
  函数体A       函数体B
```

系统按数组顺序依次调用每个回调，传入触发原因（`DLL_PROCESS_ATTACH` 等）。

### SizeOfZeroFill

TLS 模板数据之后额外用 **0** 填充的字节数。对应 C/C++ 中声明了但未初始化的 `__declspec(thread)` 变量——编译器把它们放在模板区末尾，初始值全零，这部分不需要写入文件，由加载器按此字段大小补零即可。

### Characteristics

当前版本保留，固定为 0，解析时可忽略。

## TLS 回调函数

TLS 回调是整张 TLS 表中**逆向分析最需关注的部分**。

### 回调函数原型

```c
void NTAPI TlsCallback(PVOID DllHandle, DWORD Reason, PVOID Reserved)
{
    switch(Reason)
    {
        case DLL_PROCESS_ATTACH:
            // 进程初始化时调用
            break;
        case DLL_THREAD_ATTACH:
            // 线程创建时调用
            break;
        case DLL_THREAD_DETACH:
            // 线程退出时调用
            break;
        case DLL_PROCESS_DETACH:
            // 进程终止时调用
            break;
    }
}
```

参数与 `DllMain` 完全一致，`DllHandle` 是模块基址，`Reason` 是触发时机，`Reserved` 保留（静态加载时为非 NULL）。

### 注册回调函数（编译器方式）

```c
// TLS回调函数示例
void NTAPI TlsCallback(PVOID DllHandle, DWORD Reason, PVOID Reserved)
{
    switch(Reason)
    {
        case DLL_THREAD_ATTACH:
            printf("新线程创建\n");
            break;
        case DLL_THREAD_DETACH:
            printf("线程即将退出\n");
            break;
    }
}

// 在PE文件中注册TLS回调
#pragma data_seg(".CRT$XLB")
PIMAGE_TLS_CALLBACK pTlsCallback = TlsCallback;
#pragma data_seg()
```

### TLS 回调执行时机（重点）

> [!important] TLS 回调先于入口点执行
> Windows 加载器在将控制权交给 `WinMain`/`DllMain` **之前**，就会遍历 `AddressOfCallBacks` 数组，依次用 `DLL_PROCESS_ATTACH` 调用每个 TLS 回调。这意味着 TLS 回调是程序中"最先执行"的用户代码。

```text
进程/线程生命周期与 TLS 回调时机
────────────────────────────────────────────────────────────
进程启动
  │
  ▼
加载器映射 PE 到内存
  │
  ▼
处理静态导入（IAT 修补） ────────────────────────────────┐
  │                                                      │
  ▼                                                      │
遍历 TLS 回调数组                                        │
  ├── 调用 Callback1(DLL_PROCESS_ATTACH)                │  ← ★ 在入口点之前
  ├── 调用 Callback2(DLL_PROCESS_ATTACH)                │
  └── ... 直到遇到 NULL                                 │
  │                                                      │
  ▼                                                      │
调用 WinMain / DllMain(DLL_PROCESS_ATTACH) ◄────────────┘
  │
  ▼
程序正常运行...
  │
  ├── 新线程创建时：TLS 回调(DLL_THREAD_ATTACH) → 线程入口
  │
  └── 线程退出时：TLS 回调(DLL_THREAD_DETACH) → 清理
  │
  ▼
进程退出：TLS 回调(DLL_PROCESS_DETACH) → ExitProcess
────────────────────────────────────────────────────────────
```

> [!warning] 逆向 / 安全提醒
> TLS 回调是恶意软件的常用"绕过"手段：
> 1. **反调试**：回调在调试器命中 `EntryPoint` 断点之前就跑完了，若调试器未配置"在 DLL 加载时暂停"，恶意代码会悄然执行完毕。
> 2. **解密/解包**：先在回调里解密后续代码，再把入口点交给解密后的壳。
> 3. **检测虚拟机/沙箱**：在入口点前先嗅探环境，若在沙箱则改写后续逻辑。
>
> **逆向分析对策**：OD/x64dbg 使用"系统断点"或"DLL 入口"停驻点，IDA 检查 `.tls` 节与 `AddressOfCallBacks` 处的函数指针，不要仅靠 `AddressOfEntryPoint` 作为分析起点。

## TLS 的工作机制

### 线程创建时的初始化流程

```text
CreateThread() / 系统创建新线程
         │
         ▼
  为新线程分配 TLS 存储块
  大小 = (EndAddr - StartAddr) + SizeOfZeroFill
         │
         ▼
  将 [StartAddr, EndAddr) 的模板数据复制到 TLS 块
         │
         ▼
  对后续 SizeOfZeroFill 字节填零
         │
         ▼
  调用所有 TLS 回调(DLL_THREAD_ATTACH)
         │
         ▼
  执行线程入口函数（ThreadProc）
```

### TLS 数据的访问路径

| 访问方式 | 说明 |
|----------|------|
| `__declspec(thread)` 变量 | 编译器直接生成访问 TLS 节的指令，性能最优 |
| `TlsAlloc` / `TlsGetValue` / `TlsSetValue` / `TlsFree` | Win32 API，动态分配 TLS 槽位，兼容性更好 |
| TEB 直接访问 | 内核/驱动层直接读 `FS:[0x2C]`（x86）或 `GS:[0x58]`（x64）的 TLS 指针，极少在应用层用 |

Windows 内部实现中，每个线程的 TLS 数据指针存放在 **TEB**（线程环境块）的 `TlsSlots` 数组里，加载器通过 `AddressOfIndex` 字段写入的槽位编号来寻址：

```text
TEB (每线程一个)
┌────────────────────────────┐
│  ...                       │
│  TlsSlots[64]              │  ← FS:[0x2C] (x86) / GS:[0x58] (x64)
│    [0] → 模块A的TLS块指针  │
│    [1] → 模块B的TLS块指针  │
│    ...                     │
│  TlsExpansionSlots*        │  ← 超过 64 个时的扩展区
│  ...                       │
└────────────────────────────┘
```

## 实际应用

### 在 C/C++ 中声明 TLS 变量

```c
// 使用__declspec(thread)声明线程局部变量
__declspec(thread) int threadLocalVar = 0;
__declspec(thread) char threadLocalBuffer[256];
```

编译器会自动将这些变量放入 `.tls` 节，并生成对应的 `IMAGE_TLS_DIRECTORY` 描述。

### 不同场景下的 TLS 用途

| 场景 | 典型用途 |
|------|----------|
| DLL | 为调用者的每个线程维护独立句柄/状态（如 errno、locale 设置） |
| EXE | 多线程应用的每线程运行时状态，避免全局锁竞争 |
| 安全/加密库 | 线程私有密钥缓冲区，防止跨线程泄漏 |
| 恶意软件 | TLS 回调作为"前置执行"钩子，实现反调试、解密壳 |

> [!note] TLS 与 DLL 加载顺序
> 静态 TLS（`__declspec(thread)`）要求 DLL 在进程启动时就已加载，**不支持 `LoadLibrary` 动态加载后使用**（系统只在进程启动时为已知模块分配 TLS 块）。若需动态加载的 DLL 使用 TLS，应改用 `TlsAlloc` / `TlsGetValue` API 方式。

## 工具查看与实战分析

### 使用 dumpbin 查看

```
TLS Table
  StartAddressOfRawData: 0000000140006000
  EndAddressOfRawData:   0000000140006010
  AddressOfIndex:        0000000140007000
  AddressOfCallBacks:    0000000140007008
  SizeOfZeroFill:        00000000
  Characteristics:       00000000
```

- `StartAddressOfRawData` = `0x140006000`，`End` = `0x140006010`：说明模板数据大小为 16 字节。
- `AddressOfCallBacks` = `0x140007008`：跳到此地址读函数指针数组，遇 NULL 停止，即得所有 TLS 回调。
- `SizeOfZeroFill` = 0：没有额外零填充区，线程 TLS 块大小就是 16 字节。

### 手工解析要点

```text
1. 从 Optional Header 的 DataDirectory[9] 读取 TLS 目录 RVA
2. RVA → 文件偏移（参见 [[07-详解PE文件(七)：数据目录|数据目录]] 节区换算方法）
3. 按 IMAGE_TLS_DIRECTORY32/64 结构读取 6 个字段
4. 若 AddressOfCallBacks != 0，逐个读取函数指针直到遇到 NULL
5. 将每个回调 RVA 转换为文件偏移，反汇编查看内容
```

> [!example] 典型十六进制片段（PE32+，64 位）
> ```text
> 偏移        字节（小端）                             字段
> 00000A00    00 60 00 40 01 00 00 00   StartAddressOfRawData = 0x140006000
> 00000A08    10 60 00 40 01 00 00 00   EndAddressOfRawData   = 0x140006010
> 00000A10    00 70 00 40 01 00 00 00   AddressOfIndex        = 0x140007000
> 00000A18    08 70 00 40 01 00 00 00   AddressOfCallBacks    = 0x140007008
> 00000A20    00 00 00 00               SizeOfZeroFill        = 0
> 00000A24    00 00 00 00               Characteristics       = 0
> ```

> [!summary] 小结
> TLS 表（数据目录索引 9）是 PE 文件中多线程支持的底层描述结构。核心由 `IMAGE_TLS_DIRECTORY` 的 6 个字段组成：模板数据区间（`Start/EndAddressOfRawData`）、TLS 槽位索引指针（`AddressOfIndex`）、回调函数数组指针（`AddressOfCallBacks`）以及零填充大小（`SizeOfZeroFill`）。
>
> **最值得记住的两点**：
> 1. 每个线程创建时，系统自动复制模板数据并依次调用所有 TLS 回调。
> 2. TLS 回调**先于** `WinMain`/`DllMain` 执行，是逆向分析必须首先检查的执行点。
>
> 相关篇目：[[07-详解PE文件(七)：数据目录|数据目录总览]] · [[06-详解PE文件(六)：可选头|可选头]] · [[18-详解PE文件(十八)：加载配置表|加载配置表（下一篇）]]

---

← 上一篇：[[16-详解PE文件(十六)：全局指针]]　｜　[[00-合集总览-PE文件详解|📚 返回合集总览]]　｜　下一篇：[[18-详解PE文件(十八)：加载配置表]] →
