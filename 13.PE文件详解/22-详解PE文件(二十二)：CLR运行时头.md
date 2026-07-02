---
title: "详解PE文件(二十二)：CLR运行时头"
series: PE文件详解
order: 22
source: https://blog.csdn.net/geesehoward20000/article/details/154745184
author: geesehoward20000（阿捏利）
published: 2025-11-28
collected: 2026-06-26
aliases:
  - CLR运行时头
  - IMAGE_COR20_HEADER
  - PE(二十二)
summary: .NET 程序集专属的 PE 扩展头（IMAGE_COR20_HEADER，72 字节），位于数据目录第 15 项，是 CLR 定位元数据、入口点、强名称签名等托管运行时信息的唯一入口。
tags:
  - PE文件
  - Windows
  - 逆向工程
  - 二进制文件格式
  - 数据目录
---
# 详解PE文件(二十二)：CLR运行时头

> 来源：[CSDN 原文](https://blog.csdn.net/geesehoward20000/article/details/154745184)　｜　发布：2025-11-28　｜　合集《PE文件详解》第 22/26 篇

> [!tip] 一句话核心
> CLR 运行时头（`IMAGE_COR20_HEADER`，固定 **72 字节**）是 .NET 程序集专属的 PE 扩展：它藏在 [[07-详解PE文件(七)：数据目录|数据目录]] 第 15 个条目（`IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR`）里，告诉 CLR 去哪里找元数据、入口点和强名称签名——**有它，PE 就是托管程序集；没它，PE 就是普通原生文件**。

> [!info] 速查
> - **结构体**：`IMAGE_COR20_HEADER`（也叫 CLI Header / COM+ Header）
> - **大小**：固定 **72 字节（0x48）**
> - **位置**：[[07-详解PE文件(七)：数据目录|数据目录]] 第 15 项（索引 14，`IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR`）
> - **必查字段**：`cb`（结构大小）、`MetaData`（元数据 RVA+Size）、`Flags`（运行模式）、`EntryPointToken/RVA`（入口点）
> - **判断 .NET 程序集**：数据目录第 15 项 VirtualAddress ≠ 0 即为托管程序集
> - **互链**：[[07-详解PE文件(七)：数据目录]]、[[06-详解PE文件(六)：可选头]]

.NET 诞生于 2000 年代初，微软需要让 CLR（Common Language Runtime，公共语言运行时）能够识别并加载托管程序集，但又不破坏现有的 PE 加载体系——于是 `IMAGE_COR20_HEADER` 应运而生。它被"嵌入"进数据目录的第 15 个槽位（这个槽原先叫 COM Descriptor，所以也叫 COM+ 运行时头），原生 PE 加载器对它完全无感，而 CLR 加载器则把它作为进入托管世界的第一把钥匙。

## CLR 运行时头在 PE 文件中的位置

理解 CLR 运行时头，首先要看清它在整个 PE 结构中的坐标：

```text
PE 文件整体布局（.NET 程序集视角）
─────────────────────────────────────────────────────
0x00  ┌────────────────────────────────────────────┐
      │  DOS 头 + DOS Stub                          │
0x??  ├────────────────────────────────────────────┤
      │  PE 签名 "PE\0\0"                           │
      ├────────────────────────────────────────────┤
      │  COFF 文件头                                │
      ├────────────────────────────────────────────┤
      │  可选头（Optional Header）                  │
      │    └── DataDirectory[14]  ─────────────────┼──┐  ← 第 15 项
      ├────────────────────────────────────────────┤  │
      │  节表（节描述符数组）                        │  │
      ├────────────────────────────────────────────┤  │
      │  .text 节                                   │  │
      │    ┌────────────────────────────────────┐  │  │
      │    │  IMAGE_COR20_HEADER（72 字节）      │◄─┼──┘
      │    │    └── MetaData  ──────────────────┼──┼──┐
      │    └────────────────────────────────────┘  │  │
      │    │  IL 代码 / 混合原生代码               │  │
      ├────────────────────────────────────────────┤  │
      │  .clr 节 / #~ 元数据流                     │◄─┼──┘
      │    （类型定义、方法签名、字符串堆等）        │  │
      └────────────────────────────────────────────┘
```

> [!note] 数据目录第 15 项是什么？
> [[06-详解PE文件(六)：可选头|可选头]] 末尾有 16 个 `IMAGE_DATA_DIRECTORY` 条目（每条 8 字节：4 字节 RVA + 4 字节 Size），其中第 15 项（索引 14）专门预留给 CLR。当这一项的 `VirtualAddress` 不为 0 时，说明该 PE 文件是一个 .NET 程序集，CLR 加载器随即按 RVA 定位 `IMAGE_COR20_HEADER`。详见 [[07-详解PE文件(七)：数据目录]]。

## 原生 PE vs .NET PE 加载路径对比

同一个 PE 文件，Windows 加载器与 CLR 加载器的处理路径截然不同：

```text
                       PE 文件加载流程分叉点
                              │
                    读数据目录第 15 项
                              │
              ┌───────────────┴───────────────┐
              │ VirtualAddress == 0            │ VirtualAddress ≠ 0
              ▼                               ▼
         原生 PE 加载                    .NET 程序集加载
    ┌──────────────────┐          ┌──────────────────────────┐
    │ 解析导入表        │          │ 加载 CLR 运行时（mscoree.dll）│
    │ 基址重定位        │          │ 读 IMAGE_COR20_HEADER     │
    │ 直接执行原生代码  │          │  └─ 定位 MetaData          │
    └──────────────────┘          │  └─ 验证 StrongName        │
                                  │  └─ 定位入口点 Token/RVA   │
                                  │ JIT 编译 IL → 原生代码     │
                                  │ 执行托管代码               │
                                  └──────────────────────────┘
```

> [!important] .NET PE 的"双重身份"
> 一个 .NET 程序集**同时是合法的 PE 文件**——`ntdll.dll` 仍然按标准 PE 流程映射文件、处理导入表；只是当 Windows 发现数据目录第 15 项非空时，会让执行流转入 `mscoree.dll` 的 `_CorExeMain` / `_CorDllMain`，由 CLR 接管后续的 JIT 编译与托管执行。

## IMAGE_COR20_HEADER 结构定义

以下结构体定义**逐字保留原文**，不做任何修改：

```c
typedef struct IMAGE_COR20_HEADER
{
    // Header versioning
    DWORD cb;                               // 结构体大小
    WORD  MajorRuntimeVersion;              // 主版本号
    WORD  MinorRuntimeVersion;              // 次版本号

    // Symbol table and startup information
    IMAGE_DATA_DIRECTORY MetaData;          // 元数据目录
    DWORD Flags;                            // 标志位

    union {
        DWORD EntryPointToken;              // 入口点的元数据token
        DWORD EntryPointRVA;                // 入口点的相对虚拟地址（RVA）
    } DUMMYUNIONNAME;

    // Binding information
    IMAGE_DATA_DIRECTORY Resources;         // 资源目录
    IMAGE_DATA_DIRECTORY StrongNameSignature; // 强名称签名

    // Regular fixup and binding information
    IMAGE_DATA_DIRECTORY CodeManagerTable;  // 代码管理器表
    IMAGE_DATA_DIRECTORY VTableFixups;      // 虚函数表修复
    IMAGE_DATA_DIRECTORY ExportAddressTableJumps; // 导出地址表跳转

    // Precompiled image info (internal use only - set to zero)
    IMAGE_DATA_DIRECTORY ManagedNativeHeader; // 托管本机头
} IMAGE_COR20_HEADER, *PIMAGE_COR20_HEADER;
```

## 字段偏移布局表

将 72 字节按偏移摊开，快速定位每个字段：

| 偏移 | 字段 | 大小 | 说明 |
|------|------|------|------|
| `0x00` ⭐ | `cb` | 4 | 结构体自身大小，通常为 `0x48`（72） |
| `0x04` | `MajorRuntimeVersion` | 2 | CLR 主版本号（.NET 2.0 起通常为 `2`） |
| `0x06` | `MinorRuntimeVersion` | 2 | CLR 次版本号（通常为 `5`） |
| `0x08` ⭐ | `MetaData.VirtualAddress` | 4 | 元数据流的 RVA |
| `0x0C` | `MetaData.Size` | 4 | 元数据流的字节大小 |
| `0x10` ⭐ | `Flags` | 4 | 标志位（见下节） |
| `0x14` ⭐ | `EntryPointToken / EntryPointRVA` | 4 | 托管/混合模式入口点（union） |
| `0x18` | `Resources.VirtualAddress` | 4 | 嵌入资源的 RVA |
| `0x1C` | `Resources.Size` | 4 | 嵌入资源的大小 |
| `0x20` | `StrongNameSignature.VirtualAddress` | 4 | 强名称签名 RVA |
| `0x24` | `StrongNameSignature.Size` | 4 | 强名称签名大小 |
| `0x28` | `CodeManagerTable.VirtualAddress` | 4 | 代码管理器表 RVA（通常为 0） |
| `0x2C` | `CodeManagerTable.Size` | 4 | 代码管理器表大小 |
| `0x30` | `VTableFixups.VirtualAddress` | 4 | 虚函数表修复 RVA（混合模式使用） |
| `0x34` | `VTableFixups.Size` | 4 | 虚函数表修复大小 |
| `0x38` | `ExportAddressTableJumps.VirtualAddress` | 4 | 导出地址跳转 RVA |
| `0x3C` | `ExportAddressTableJumps.Size` | 4 | 导出地址跳转大小 |
| `0x40` | `ManagedNativeHeader.VirtualAddress` | 4 | 托管本机头 RVA（仅内部使用，通常为 0） |
| `0x44` | `ManagedNativeHeader.Size` | 4 | 托管本机头大小 |

> [!note] 为什么大小是 72 而不是更规整的数？
> `IMAGE_COR20_HEADER` 包含 1 个 `DWORD`（cb）、2 个 `WORD`（版本号）、7 个 `IMAGE_DATA_DIRECTORY`（各 8 字节）共 4 + 4 + 7×8 = 64 字节，再加上 1 个 union `DWORD`（4 字节）以及 `Flags` 的 `DWORD`（4 字节）—— 实际布局是 `4+2+2+8+4+4+8×6 = 72`，即 `0x48`。

## 各字段详解

### 基本版本信息：cb / MajorRuntimeVersion / MinorRuntimeVersion

- **`cb`**：结构体自身大小，固定为 `0x48`（72 字节）。解析器可据此检验头部合法性——若不是 72，说明数据损坏或格式变种。
- **`MajorRuntimeVersion`** / **`MinorRuntimeVersion`**：指定执行此程序集所需的最低 CLR 版本。常见取值：

| 值（Major.Minor） | 对应 .NET 版本 |
|---|---|
| 2.0 | .NET Framework 1.0 / 1.1 |
| 2.5 | .NET Framework 2.0 / 3.0 / 3.5 |
| 4.0 | .NET Framework 4.x |
| 5.0 | .NET 5 / .NET 6 / .NET 7+ |

### 元数据入口：MetaData（IMAGE_DATA_DIRECTORY）

`MetaData` 是整个 CLR 运行时头中**最核心**的字段——它的 `VirtualAddress` 指向元数据根（Metadata Root），从那里可以访问所有的元数据流（`#~`、`#Strings`、`#GUID`、`#Blob`、`#US`），进而解析出程序集中的类型、方法、字段、自定义属性等完整的类型系统信息。

```text
MetaData RVA ──▶  Metadata Root（"BSJB" 签名）
                       │
          ┌────────────┼────────────┬─────────────┐
          ▼            ▼            ▼             ▼
       #~流        #Strings堆    #GUID堆       #Blob堆
    （压缩的        （方法名/       （GUID         （签名/
     表格流）        类型名等）      数组）         自定义属性）
```

### 标志位：Flags

`Flags` 是一个 32 位位域，控制程序集的加载行为：

| 标志名 | 值 | 含义 |
|---|---|---|
| `COMIMAGE_FLAGS_ILONLY` | `0x00000001` | 程序集**只含 IL**，无原生代码 |
| `COMIMAGE_FLAGS_32BITREQUIRED` | `0x00000002` | 必须在 32 位进程中运行 |
| `COMIMAGE_FLAGS_IL_LIBRARY` | `0x00000004` | 作为库（通常不单独使用） |
| `COMIMAGE_FLAGS_STRONGNAMESIGNED` | `0x00000008` | 已用强名称签名 |
| `COMIMAGE_FLAGS_NATIVE_ENTRYPOINT` | `0x00000010` | 入口点为原生 RVA（混合模式） |
| `COMIMAGE_FLAGS_TRACKDEBUGDATA` | `0x00010000` | 调试时跟踪数据 |
| `COMIMAGE_FLAGS_32BITPREFERRED` | `0x00020000` | 优先 32 位但允许 64 位（AnyCPU prefer-32bit） |

> [!warning] Flags 对安全分析的意义
> - `ILONLY` 未置位说明混有原生代码，反汇编时不能只看 IL，需同时分析原生函数体。
> - `STRONGNAMESIGNED` 置位但签名校验失败是程序集被篡改的强信号。
> - 恶意软件有时将 `NATIVE_ENTRYPOINT` 置位，让入口点绕过托管安全检查直接跳入原生 shellcode，是分析时需重点关注的 red flag。

### 入口点：EntryPointToken / EntryPointRVA（union）

这是一个 4 字节 union，根据 `Flags` 中 `COMIMAGE_FLAGS_NATIVE_ENTRYPOINT` 位来区分语义：

```text
COMIMAGE_FLAGS_NATIVE_ENTRYPOINT == 0  →  EntryPointToken
    托管入口点：元数据方法定义 Token（如 0x06000001 = MethodDef 第 1 行）
    CLR 通过 Token 在元数据表中找到对应的方法，JIT 编译后执行

COMIMAGE_FLAGS_NATIVE_ENTRYPOINT == 1  →  EntryPointRVA
    原生入口点：文件中的 RVA，直接指向原生机器码
    用于混合模式（C++/CLI）程序集，CLR 直接跳转执行
```

### 强名称签名：StrongNameSignature

强名称（Strong Name）是 .NET 程序集的完整性与唯一性机制，由**公钥 + 版本号 + 文化 + 哈希签名**组成。`StrongNameSignature` 字段给出签名数据在文件中的 RVA 和大小；`Flags` 的 `STRONGNAMESIGNED` 位则标记该签名是否已完成写入。

> [!note] 强名称 ≠ 代码签名证书
> 强名称（Strong Name）是 CLR 内部的程序集标识机制，使用公钥/私钥对文件内容做 hash + 加密；Authenticode 代码签名（证书表，见 [[12-详解PE文件(十二)：证书表]]）是 Windows 内核验证 PE 来源的机制。二者独立共存，互不替代。

### 混合模式相关字段

| 字段 | 用途 |
|---|---|
| `Resources` | 嵌入的托管资源（如 RESX 编译进去的二进制资源），与 [[10-详解PE文件(十)：资源表|Win32 资源表]] 不同 |
| `CodeManagerTable` | 代码管理器表（早期设计，现代 CLR 已弃用，通常全零） |
| `VTableFixups` | 混合模式（C++/CLI）中，原生代码通过虚函数表调用托管方法时的修复信息 |
| `ExportAddressTableJumps` | 混合模式程序集以 COM 方式暴露函数时使用 |
| `ManagedNativeHeader` | 预编译（NGen）镜像的内部信息，纯 IL 程序集此处全零 |

## 元数据与 IL 的关系

`IMAGE_COR20_HEADER` 本身不包含任何类型信息或 IL 指令——它只是一张"索引图"，真正的内容在它指向的两大区域：

```text
IMAGE_COR20_HEADER
  │
  ├─── MetaData ──▶  元数据根（Metadata Root）
  │                     存类型/方法/字段的"目录"
  │                     #~ 表格：MethodDef 行存各方法的 IL RVA
  │                         │
  │                         └─▶  .text 节中的 IL 方法体
  │                                方法头（fat/tiny header）+ IL 字节码
  │
  └─── EntryPointToken ──▶  元数据 MethodDef 表的某一行
                              CLR 据此找到入口方法的 IL RVA 并 JIT
```

> [!example] 十六进制解析演示：定位 CLR 运行时头
> ```text
> 步骤 1：读可选头末尾的数据目录（16 × 8 字节）
>         DataDirectory[14]（第 15 项）：
>         偏移 E0 00 00 00  ← VirtualAddress = RVA 0x000000E0
>         48 00 00 00       ← Size = 0x48（72 字节）
>
> 步骤 2：将 RVA 0xE0 转换为文件偏移（假设在 .text 节，SectionOffset = 0x200）
>         FileOffset = 0xE0 - 0x1000 + 0x200 = 0x2E0（示意）
>
> 步骤 3：读 72 字节 IMAGE_COR20_HEADER
>         48 00 00 00        cb = 0x48
>         02 00 05 00        MajorRuntimeVersion=2, MinorRuntimeVersion=5
>         6C 20 00 00        MetaData.VirtualAddress = 0x0000206C
>         3C 12 00 00        MetaData.Size = 0x123C
>         01 00 00 00        Flags = ILONLY
>         01 00 00 06        EntryPointToken = 0x06000001（MethodDef #1）
>         ...
> ```

## CLR 运行时头的作用与重要性

1. **识别 .NET 程序集**：通过 [[07-详解PE文件(七)：数据目录|数据目录]] 第 15 项是否非空，可快速判断任意 PE 文件是否为托管程序集——这比检查节名（`.text` 是否含 "BSJB"）更权威。
2. **提供运行时信息**：`MetaData`、`EntryPointToken`、`StrongNameSignature` 三个字段串联起 CLR 加载程序集的完整流程，缺一不可。
3. **支持多种运行模式**：`Flags` 一个字段即可区分纯 IL、混合模式（C++/CLI）、AnyCPU prefer-32bit 等多种部署场景。
4. **安全验证锚点**：强名称签名和 `STRONGNAMESIGNED` 标志共同构成 CLR 程序集完整性校验的基础。

## CLR 元数据流

### 元数据根（Metadata Root）结构

`IMAGE_COR20_HEADER.MetaData.VirtualAddress` 指向**元数据根**，它是整个 .NET 类型系统的入口。元数据根以固定签名 `BSJB`（Blob Signature Java Byte-stream，历史遗留名称）开头：

```text
元数据根布局
偏移   大小   字段
0x00   4      Signature       = 0x424A5342 ("BSJB")
0x04   2      MajorVersion    = 1
0x06   2      MinorVersion    = 1
0x08   4      Reserved        = 0
0x0C   4      VersionLength   = N（版本字符串长度，4 字节对齐）
0x10   N      Version         = "v4.0.30319\0..."（CLR 版本字符串，以 0 填充至对齐）
0x10+N 2      Flags           = 0（保留）
0x12+N 2      Streams         = K（流数量，通常 5）
0x14+N ...    StreamHeaders   = IMAGE_STREAM_HEADER × K（流目录）

IMAGE_STREAM_HEADER（每条）：
  DWORD  Offset   ; 流相对于元数据根的偏移
  DWORD  Size     ; 流字节大小
  CHAR   Name[]   ; 流名（变长 ASCII，4 字节对齐，带 \0 结尾）
```

**常见流目录示例：**

```text
StreamHeaders（示例）：
  Offset=0x6C, Size=0x3A4C, Name="#~"
  Offset=0x3AB8, Size=0x1C40, Name="#Strings"
  Offset=0x56F8, Size=0x30,   Name="#US"
  Offset=0x5728, Size=0x10,   Name="#GUID"
  Offset=0x5738, Size=0x8C0,  Name="#Blob"
```

### 五大元数据流详解

.NET 元数据分成五个逻辑流，各司其职：

| 流名 | 全称 | 内容 | 在反编译中的作用 |
|------|------|------|----------------|
| `#~` | Compressed Metadata Tables | 压缩格式的表格集合（类型定义/方法/字段等） | **核心**：所有类/方法/字段的结构化数据 |
| `#Strings` | String Heap | UTF-8 字符串堆（以 `\0` 分隔） | 类名、方法名、字段名的存储池 |
| `#US` | User Strings Heap | UTF-16 用户字符串（`ldstr` 指令引用） | 源码中的字符串字面量（`"Hello"` 等） |
| `#GUID` | GUID Heap | 16 字节 GUID 数组 | 模块 GUID、接口标识 |
| `#Blob` | Blob Heap | 任意二进制 blob（方法签名、自定义属性、常量等） | 方法签名、泛型约束、自定义 Attribute 的值 |

> [!note] `#-`（非压缩流）
> 除 `#~`（压缩）外，还存在 `#-`（非压缩/通用格式），内容相同但各表独立存储，常见于某些旧版编译器或混淆工具输出的程序集；`#~` 是现代编译器（Roslyn）的默认选择。

#### `#~` 流：压缩元数据表

`#~` 是最复杂的流，内部由多张表组成，以压缩二进制格式存储：

```text
#~ 流布局：
  偏移   字段
  0x00   Reserved      = 0
  0x04   MajorVersion  = 2
  0x05   MinorVersion  = 0
  0x06   HeapSizes     ; 位标志：bit0=StringHeap>64KB, bit1=GUIDHeap>64KB, bit2=BlobHeap>64KB
  0x07   Reserved      = 1
  0x08   Valid         ; 64 位掩码，每位对应一张表是否存在
  0x10   Sorted        ; 64 位掩码，每张表是否排序
  0x18   Rows[]        ; 存在的表的行数（DWORD × popcount(Valid)）
  ...    Tables[]      ; 各表的行数据（行宽度由各列类型和 HeapSizes 决定）
```

**常用表（Table ID）：**

| Table ID | 表名 | 主要列 |
|----------|------|--------|
| 0x00 | `Module` | Name（StringIdx）、Mvid（GUIDIdx） |
| 0x01 | `TypeRef` | ResolutionScope、Name、Namespace |
| 0x02 | `TypeDef` | Flags、Name、Namespace、Extends、FieldList、MethodList |
| 0x04 | `Field` | Flags、Name、Signature（BlobIdx） |
| 0x06 | `MethodDef` | RVA、ImplFlags、Flags、Name、Signature、ParamList |
| 0x0A | `MemberRef` | Class、Name、Signature |
| 0x0B | `Constant` | Type、Parent、Value（BlobIdx） |
| 0x14 | `StandAloneSig` | Signature（BlobIdx） |
| 0x23 | `Assembly` | HashAlgId、MajorVersion、…、Name、Culture |
| 0x24 | `AssemblyRef` | 引用外部程序集的版本+名称+公钥 token |

> [!tip] 表行 Token 格式
> CLR 中每个元数据实体用一个 32 位 Token 引用：`高 8 位 = Table ID`，`低 24 位 = 行号（1-based）`。  
> 例如：`0x06000001` = MethodDef 表第 1 行（即第一个方法），正是 `EntryPointToken` 的典型值。

#### `#Strings` 堆：类型名/方法名的存储池

```text
#Strings 堆布局：字节流，相邻字符串以 \0 分隔
  偏移 0x00: \0                       （第一个字节恒为 \0，代表空字符串）
  偏移 0x01: "Program\0"
  偏移 0x09: "Main\0"
  偏移 0x0E: "System\0"
  ...

引用方式：#~ 表的 StringIdx 列存储字节偏移，加载器到此偏移处读 \0 结尾的字符串
```

#### `#US` 堆：源码字符串字面量

`ldstr` IL 指令的操作数是指向 `#US` 堆的 Token（`0x70xxxxxx`）：

```text
#US 堆布局：
  每条目 = 前缀长度（7 位压缩整数，字节数，含末尾标志位） + UTF-16LE 字符串字节 + 1 字节尾标志
  
  例："Hello" (5 chars = 10 bytes UTF-16 + 1 flag = 11 bytes total):
    0B                     ; 长度 = 11
    48 00 65 00 6C 00 6C 00 6F 00   ; "Hello" UTF-16LE
    01                     ; 尾标志（1 = 包含非 ASCII 字符，0 = 纯 ASCII）
```

逆向时，`#US` 堆是提取**明文字符串**（URL、命令、密钥片段）的第一优先级目标。

### EntryPointToken vs EntryPointRVA：托管 vs 混合模式

`IMAGE_COR20_HEADER` 中 `EntryPointToken/RVA` 字段（偏移 `0x14`）是一个 union，由 `Flags` 中的 `COMIMAGE_FLAGS_NATIVE_ENTRYPOINT`（`0x10`）位决定语义：

```text
COMIMAGE_FLAGS_NATIVE_ENTRYPOINT == 0（大多数纯托管程序集）
────────────────────────────────────────────────────────
EntryPointToken = 托管方法 Token（格式 0x06xxxxxx）
  └─▶ 查 MethodDef 表第 xxxxxx 行
        ├─ RVA 字段 → IL 方法体在 .text 节中的 RVA
        └─ Name → 方法名（通常 "Main" 或 "<Main>$"）
  CLR JIT 编译该 IL → 执行

示例值：0x06000001 = MethodDef 表第 1 行 = 通常是 Program::Main

COMIMAGE_FLAGS_NATIVE_ENTRYPOINT == 1（C++/CLI 混合模式）
────────────────────────────────────────────────────────
EntryPointRVA = 原生机器码入口点 RVA（不经 JIT）
  └─▶ 直接指向 .text 中的原生 x86/x64 代码
  CLR 直接跳转执行（用于 C++/CLI，允许托管/非托管互操作）
```

**判断入口类型的代码：**

```c
DWORD flags = pCor20->Flags;
DWORD entry = pCor20->EntryPointToken; // union 中的值
if (flags & COMIMAGE_FLAGS_NATIVE_ENTRYPOINT) {
    printf("原生入口点 RVA = 0x%08X\n", entry);
} else {
    DWORD tableId  = (entry >> 24) & 0xFF;  // 应为 0x06 (MethodDef)
    DWORD rowIndex = entry & 0x00FFFFFF;     // 1-based 行号
    printf("托管入口 Token = 0x%08X (Table=%02X, Row=%u)\n",
           entry, tableId, rowIndex);
}
```

### .NET 反编译入口：从 CLR 头到 IL

掌握元数据流后，反编译工具（ILSpy / dnSpy / de4dot）的工作路径可归纳为：

```text
.NET 程序集反编译路径
═══════════════════════════════════════════════════════════════

① 读 DataDirectory[14] → IMAGE_COR20_HEADER
         │
         ▼
② MetaData.VirtualAddress → 元数据根（"BSJB"）
         │
         ├─ StreamHeader "#~"  → 所有元数据表（类型/方法/字段清单）
         │       │
         │       └─ MethodDef 表每行的 RVA → IL 方法体（.text 中）
         │               │
         │               └─ IL 字节码 → 反编译为 C# / VB.NET 伪源码
         │
         ├─ StreamHeader "#Strings" → 类名/方法名/字段名
         ├─ StreamHeader "#US"      → 字符串字面量（提取敏感字符串）
         ├─ StreamHeader "#GUID"    → 模块标识 GUID
         └─ StreamHeader "#Blob"    → 方法签名、自定义属性、常量值

③ EntryPointToken → MethodDef 表某行 → IL RVA → 入口 IL 方法体
         │
         ▼
④ JIT（运行时）或反编译工具（静态）将 IL 还原为高级语言
```

**常用反编译工具与其入口点定位方式：**

| 工具 | 定位入口点方式 | 特点 |
|------|--------------|------|
| **dnSpy** | 自动解析 `EntryPointToken`，Entry Point 标注在 UI 中 | 支持 IL 与 C# 双视图，可调试 |
| **ILSpy** | 同上，左侧树状视图中 `[entrypoint]` 标注 | 开源，Roslyn 反编译质量高 |
| **de4dot** | 先去混淆再交给 ILSpy/dnSpy | 处理 .NET 混淆器（ConfuserEx、SmartAssembly 等） |
| **ildasm.exe** | `MSIL DISASSEMBLER` - `.entrypoint` 标注 | 微软官方，输出 IL 文本 |
| **Ghidra** | 需要 .NET 插件；元数据解析后跳入 IL → p-code | 支持混合模式程序集的原生代码分析 |

**用 ildasm 快速定位入口：**

```bash
# 输出包含 .entrypoint 伪指令的方法即为入口
ildasm /text /tok hello.exe | findstr /C:".entrypoint" /C:"IL_0000"

# 查看所有方法的 IL 代码（含 Token）
ildasm /output=hello.il hello.exe
```

> [!example] dnSpy 实战：定位混淆程序集的真实入口
> 1. 打开 dnSpy → File → Open → 拖入目标 .exe
> 2. 左侧树：`<module>` → `<namespace>` → 找带 `[entrypoint]` 标注的方法
> 3. 若被混淆（类名/方法名为随机字符），用 Edit → Find → 搜索 `[entrypoint]`
> 4. 查看 IL 代码（View → IL）确认 `newobj` / `call` 的目标 Token，追溯真实逻辑
> 5. 若 `COMIMAGE_FLAGS_NATIVE_ENTRYPOINT` 置位，切换到汇编视图分析原生代码

> [!summary] 小结
> `IMAGE_COR20_HEADER` 是 .NET 程序集在 PE 格式上的唯一扩展点，也是原生 PE 世界与托管 CLR 世界的分界线。**记住三个核心字段**：`MetaData`（找到所有类型信息的起点）、`Flags`（决定运行模式）、`EntryPointToken/RVA`（程序的第一行代码在哪）。回顾全局：[[07-详解PE文件(七)：数据目录]] 第 15 项的 RVA 指向本结构；[[06-详解PE文件(六)：可选头]] 决定了整个数据目录数组的位置。理解 CLR 运行时头，是分析 .NET 程序集、做 IL 级逆向、排查强名称验证失败的必备基础。

---

← 上一篇：[[21-详解PE文件(二十一)：延迟导入表]]　｜　[[00-合集总览-PE文件详解|📚 返回合集总览]]　｜　下一篇：[[23-详解PE文件(二十三)：保留]] →
