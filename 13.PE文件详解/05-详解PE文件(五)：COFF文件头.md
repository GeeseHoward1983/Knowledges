---
title: "详解PE文件(五)：COFF文件头"
series: PE文件详解
order: 5
source: https://blog.csdn.net/geesehoward20000/article/details/154742462
author: geesehoward20000（阿捏利）
published: 2025-11-17
collected: 2026-06-26
aliases:
  - COFF文件头
  - IMAGE_FILE_HEADER
  - File Header
  - PE(五)
summary: COFF 文件头（IMAGE_FILE_HEADER）是紧跟 PE 签名的 20 字节结构，记录目标机器类型、节数量、时间戳与文件属性等关键信息。
tags:
  - PE文件
  - Windows
  - 逆向工程
  - 二进制文件格式
  - PE头部结构
---
# 详解PE文件(五)：COFF文件头

> 来源：[CSDN 原文](https://blog.csdn.net/geesehoward20000/article/details/154742462)　｜　发布：2025-11-17　｜　合集《PE文件详解》第 5/26 篇

> [!tip] 一句话核心
> COFF 文件头（`IMAGE_FILE_HEADER`）是紧跟 [[04-详解PE文件(四)：PE文件签名|PE 签名]] 的一个**固定 20 字节**结构。三件事最关键：**Machine**（给谁运行的 CPU）、**NumberOfSections**（有几个节）、**Characteristics**（是 EXE 还是 DLL 等属性）。

> [!info] 速查
> - **结构体**：`IMAGE_FILE_HEADER`
> - **大小**：固定 20 字节
> - **位置**：PE 签名之后，[[06-详解PE文件(六)：可选头|可选头]] 之前
> - **必看字段**：`Machine`、`NumberOfSections`、`Characteristics`、`SizeOfOptionalHeader`

COFF（Common Object File Format）文件头位于 PE 文件的 NT 头部分中，提供关于 PE 文件的基本信息，帮助操作系统正确加载和执行文件。

## COFF 文件头概述

COFF 文件头，也称为 `IMAGE_FILE_HEADER`，是 PE 文件 NT 头结构的一部分，紧跟在 PE 签名（"PE\0\0"）之后。它是一个固定大小为 **20 字节**的结构，包含了一些基本但关键的文件属性信息。

## COFF 文件头结构

COFF 文件头的结构定义如下：

```c
typedef struct _IMAGE_FILE_HEADER {
    WORD    Machine;                // 运行平台
    WORD    NumberOfSections;       // 文件节数量
    DWORD   TimeDateStamp;          // 时间戳
    DWORD   PointerToSymbolTable;   // 符号表偏移
    DWORD   NumberOfSymbols;        // 符号表数量
    WORD    SizeOfOptionalHeader;   // 可选头大小
    WORD    Characteristics;        // 文件属性
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;
```

按偏移摊开看这 20 字节：

| 偏移 | 字段 | 大小 | 含义 |
|---|---|---|---|
| `0x00` ⭐ | `Machine` | 2 | 目标 CPU 平台 |
| `0x02` ⭐ | `NumberOfSections` | 2 | 节的数量 |
| `0x04` | `TimeDateStamp` | 4 | 编译时间戳 |
| `0x08` | `PointerToSymbolTable` | 4 | COFF 符号表偏移（映像中通常 0） |
| `0x0C` | `NumberOfSymbols` | 4 | 符号数量（映像中通常 0） |
| `0x10` | `SizeOfOptionalHeader` | 2 | 可选头大小（32 位 0xE0 / 64 位 0xF0） |
| `0x12` ⭐ | `Characteristics` | 2 | 文件属性标志位 |

## 各字段详解

### Machine（偏移 0x00，2 字节）

指定该可执行文件的目标 CPU 平台。最常见的几个：

| 值 | 平台 |
|---|---|
| `0x014C` | Intel 386 及以上（x86） |
| `0x8664` | AMD64（x64） |
| `0xAA64` | ARM64 little endian |
| `0x01C0` | ARM little endian |
| `0x0200` | Intel IA64（Itanium） |

> [!note]- 完整 Machine 取值表（点击展开）
> - 0x014c: Intel 386处理器或更高版本
> - 0x0162: MIPS处理器
> - 0x0166: MIPS little endian (R4000)
> - 0x0168: MIPS little endian (R10000)
> - 0x0169: MIPS little endian WCI v2
> - 0x0184: Alpha AXP处理器
> - 0x01a2: Hitachi SH3处理器
> - 0x01a3: Hitachi SH3 DSP处理器
> - 0x01a6: Hitachi SH4处理器
> - 0x01a8: Hitachi SH5处理器
> - 0x01c0: ARM little endian处理器
> - 0x01c2: ARM Thumb/Thumb-2 little endian处理器
> - 0x01c4: ARM Thumb-2 little endian处理器
> - 0x01d3: AM33处理器
> - 0x01f0: PowerPC little endian处理器
> - 0x01f1: PowerPC with floating point support
> - 0x0200: Intel IA64处理器
> - 0x0266: MIPS16处理器
> - 0x0268: Motorola 68000 series
> - 0x0284: Alpha AXP 64-bit处理器
> - 0x0366: MIPS with FPU
> - 0x0466: MIPS16 with FPU
> - 0x0520: EFI字节码
> - 0x8664: AMD64 (x64)处理器
> - 0x9041: Mitsubishi M32R little endian处理器
> - 0xAA64: ARM64 little endian处理器

### NumberOfSections（偏移 0x02，2 字节）

表示 PE 文件中节（Section）的数量。这个值必须大于 0，Windows 加载器限制最多 96 个节。它直接决定了后面 [[24-详解PE文件(二十四)：节表|节表]] 有几项。

### TimeDateStamp（偏移 0x04，4 字节）

文件创建的时间戳，是从 1970 年 1 月 1 日 00:00:00 UTC 开始计算的秒数，由链接器填写。

### PointerToSymbolTable / NumberOfSymbols（偏移 0x08 / 0x0C）

COFF 符号表的文件偏移量与符号数量。在可执行映像中通常均为 0，因为调试符号信息一般放在专门的 [[14-详解PE文件(十四)：调试数据|调试数据]] 中。

### SizeOfOptionalHeader（偏移 0x10，2 字节）

指示 [[06-详解PE文件(六)：可选头|IMAGE_OPTIONAL_HEADER]] 结构的大小。对于 32 位 PE，通常是 `0x00E0`（224 字节）；对于 64 位 PE32+，通常是 `0x00F0`（240 字节）。

### Characteristics（偏移 0x12，2 字节）

表示文件的属性标志，可以是以下值的一个或多个组合：

| 标志值 | 含义 |
|---|---|
| 0x0001 | 文件中不包含重定位信息 |
| 0x0002 | 文件是可执行的 |
| 0x0004 | 行号已经被去除 |
| 0x0008 | 符号已经被去除 |
| 0x0010 | 调试信息已经被去除 |
| 0x0020 | 如果映像为一个系统文件（如内核） |
| 0x0040 | 如果映像为一个动态链接库（DLL） |
| 0x0080 | 文件应该只运行在单处理器机器上 |
| 0x0100 | 字节顺序是大端序（Big Endian） |
| 0x0200 | 目标机器基于32位字长 |
| 0x0400 | 调试信息可以局部地进行剥离 |
| 0x0800 | 在一个可移动的盘上，文件头部不能被改变 |
| 0x1000 | 如果映像为一个系统文件（如内核） |
| 0x2000 | 文件是为一个单一的32位机器上的32位字设计的 |
| 0x4000 | 数据文件 |
| 0x8000 | 如果映像为一个动态链接库（DLL） |

> [!important] 怎么判断这是 EXE 还是 DLL？
> 关键看 `Characteristics` 的 **`0x2000`（DLL）** 位与 **`0x0002`（可执行）** 位的组合：DLL 会置 DLL 位，普通 EXE 不会。这是逆向时区分模块类型最快的一招。

## 实际示例

在十六进制编辑器中查看 PE 文件时，COFF 文件头通常如下所示：

> [!example] 十六进制中的 COFF 头
> ```
> Offset      Content           Description
> 00000080                      PE签名开始
> 00000080    50 45 00 00       PE Signature = "PE\0\0"
> 00000084    4C 01             Machine = IMAGE_FILE_MACHINE_I386 (0x014C)
> 00000086    03 00             NumberOfSections = 3
> 00000088    00 00 00 00       TimeDateStamp = 0
> 0000008C    00 00 00 00       PointerToSymbolTable = 0
> 00000090    00 00 00 00       NumberOfSymbols = 0
> 00000094    E0 00             SizeOfOptionalHeader = 0xE0 (224 bytes)
> 00000096    02 01             Characteristics = 0x0102 (可执行文件)
> ```
> 注意小端序：`4C 01` 读作 `0x014C`、`E0 00` 读作 `0x00E0`、`02 01` 读作 `0x0102`。

## 编程访问 COFF 文件头

以下是一个简单的 C 语言示例，演示如何读取 COFF 文件头：

```c
#include <windows.h>
#include <stdio.h>
#include <winnt.h>

void PrintCOFFHeaderInfo(const char* filename) {
    HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("无法打开文件: %s\n", filename);
        return;
    }

    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        printf("创建文件映射失败\n");
        CloseHandle(hFile);
        return;
    }

    LPVOID lpBase = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!lpBase) {
        printf("映射文件失败\n");
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return;
    }

    // 获取DOS头
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)lpBase;

    // 验证DOS签名
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        printf("不是有效的PE文件\n");
        UnmapViewOfFile(lpBase);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return;
    }

    // 获取NT头
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)lpBase + dosHeader->e_lfanew);

    // 验证PE签名
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        printf("不是有效的PE文件\n");
        UnmapViewOfFile(lpBase);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return;
    }

    // 获取COFF文件头
    IMAGE_FILE_HEADER* fileHeader = &ntHeaders->FileHeader;

    // 打印COFF文件头信息
    printf("=== COFF文件头信息 ===\n");
    printf("Machine: 0x%04X\n", fileHeader->Machine);
    printf("NumberOfSections: %d\n", fileHeader->NumberOfSections);
    printf("TimeDateStamp: 0x%08X\n", fileHeader->TimeDateStamp);
    printf("PointerToSymbolTable: 0x%08X\n", fileHeader->PointerToSymbolTable);
    printf("NumberOfSymbols: %d\n", fileHeader->NumberOfSymbols);
    printf("SizeOfOptionalHeader: 0x%04X\n", fileHeader->SizeOfOptionalHeader);
    printf("Characteristics: 0x%04X\n", fileHeader->Characteristics);

    UnmapViewOfFile(lpBase);
    CloseHandle(hMapping);
    CloseHandle(hFile);
}
```

> [!summary] 小结
> COFF 文件头是紧跟 [[04-详解PE文件(四)：PE文件签名|PE 签名]] 的 20 字节结构，最该记住 `Machine`（CPU 平台）、`NumberOfSections`（决定 [[24-详解PE文件(二十四)：节表|节表]] 项数）、`Characteristics`（EXE/DLL 等属性）三个字段。它和签名、[[06-详解PE文件(六)：可选头|可选头]] 一起组成 NT 头，是加载器获取文件"基本档案"的地方。

---

← 上一篇：[[04-详解PE文件(四)：PE文件签名]]　｜　[[00-合集总览-PE文件详解|📚 返回合集总览]]　｜　下一篇：[[06-详解PE文件(六)：可选头]] →
