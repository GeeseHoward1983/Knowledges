---
title: "详解PE文件(四)：PE文件签名"
series: PE文件详解
order: 4
source: https://blog.csdn.net/geesehoward20000/article/details/154742381
author: geesehoward20000（阿捏利）
published: 2025-11-17
collected: 2026-06-26
aliases:
  - PE签名
  - PE Signature
  - IMAGE_NT_SIGNATURE
  - PE(四)
summary: PE 签名是一个 4 字节固定值 "PE\0\0"（0x00004550），是 NT 头的第一个成员，由 DOS 头的 e_lfanew 指向，用于确认文件确为有效 PE。
tags:
  - PE文件
  - Windows
  - 逆向工程
  - 二进制文件格式
  - PE头部结构
---
# 详解PE文件(四)：PE文件签名

> 来源：[CSDN 原文](https://blog.csdn.net/geesehoward20000/article/details/154742381)　｜　发布：2025-11-17　｜　合集《PE文件详解》第 4/26 篇

> [!tip] 一句话核心
> PE 签名就是 4 个字节 **`50 45 00 00`（"PE\0\0"）**，它是 NT 头的第一个成员，位置由 [[02-详解PE文件(二)：DOS头|DOS 头的 e_lfanew]] 指定。加载器跳到这里，看到这 4 字节才算"二次确认"：这确实是一个 PE，可以继续往下解析了。

> [!info] 速查
> - **值**：`0x00004550`，ASCII 为 `"PE\0\0"`（小端字节序：`50 45 00 00`）
> - **大小**：4 字节
> - **位置**：由 `e_lfanew` 指向，不固定
> - **身份**：`IMAGE_NT_HEADERS` 的第一个成员 `Signature`
> - **常量**：`IMAGE_NT_SIGNATURE`

PE 文件签名用于标识文件为有效的 PE 格式。它是 NT 头（NT Headers）的第一个成员，对操作系统识别和加载 PE 文件至关重要——可以理解为继 [[02-详解PE文件(二)：DOS头|"MZ"]] 之后的**第二道门禁**。

## PE 文件签名的定义

PE 文件签名是一个 4 字节的固定值，其十六进制值为 `0x00004550`，对应的 ASCII 字符为 `"PE\0\0"`。这个签名位于 NT 头的开始位置，紧跟在 DOS 头和 [[03-详解PE文件(三)：DOS Stub程序|DOS Stub 程序]] 之后。

## PE 文件签名的结构

在 Windows SDK 的 `winnt.h` 头文件中，PE 文件签名是 NT 头结构体的第一个成员。NT 头的结构如下：

```c
typedef struct _IMAGE_NT_HEADERS {
    DWORD Signature;                    // PE文件签名
    IMAGE_FILE_HEADER FileHeader;       // 文件头
    IMAGE_OPTIONAL_HEADER32 OptionalHeader; // 可选头
} IMAGE_NT_HEADERS32, *PIMAGE_NT_HEADERS32;
```

对于 64 位 PE 文件，结构类似：

```c
typedef struct _IMAGE_NT_HEADERS64 {
    DWORD Signature;                    // PE文件签名
    IMAGE_FILE_HEADER FileHeader;       // 文件头
    IMAGE_OPTIONAL_HEADER64 OptionalHeader; // 可选头
} IMAGE_NT_HEADERS64, *PIMAGE_NT_HEADERS64;
```

> [!note] NT 头 = 签名 + COFF 头 + 可选头
> 从结构体能看清三者关系：`Signature`（本篇）+ `FileHeader`（[[05-详解PE文件(五)：COFF文件头|COFF 文件头]]）+ `OptionalHeader`（[[06-详解PE文件(六)：可选头|可选头]]）一起构成 `IMAGE_NT_HEADERS`。所以"PE 签名"既是一道门禁，也是整个 NT 头的起点。

## PE 文件签名的位置

PE 文件签名在文件中的位置不是固定的，它由 DOS 头中的 `e_lfanew` 字段指定。典型的 PE 文件结构如下：

```
+---------------------+
|   DOS Header        |  <- 64字节，包含e_lfanew字段
+---------------------+
|   DOS Stub          |  <- 可变大小的DOS程序
+---------------------+
|   PE Signature      |  <- 4字节，值为"PE\0\0"
+---------------------+
|   File Header       |  <- 20字节
+---------------------+
|   Optional Header   |  <- 可变大小（32位PE为224字节，64位PE为240字节）
+---------------------+
|   Section Table     |
+---------------------+
|   Section Data      |
+---------------------+
```

## PE 文件签名的验证

操作系统加载 PE 文件时，会执行以下验证步骤：

1. 首先检查文件开始的两个字节是否为 `"MZ"`（`0x5A4D`），确认是有效的 DOS 文件
2. 读取 DOS 头中的 `e_lfanew` 字段，获取 NT 头的偏移位置
3. 跳转到 NT 头位置，检查前 4 字节是否为 `"PE\0\0"`（`0x00004550`）
4. 如果签名匹配，则继续解析 PE 文件的其他部分

整个"双门禁"校验可以画成：

```text
  文件[0x00] == "MZ" ?  ──否──▶ 不是可执行文件
        │ 是
        ▼
  读 DOS头[0x3C] = e_lfanew
        │
        ▼
  文件[e_lfanew] == "PE\0\0" ?  ──否──▶ 不是有效 PE
        │ 是
        ▼
  继续解析 COFF 文件头 →  [05]
```

## 实际示例

在十六进制编辑器中查看 PE 文件时，NT 头开始部分通常如下所示：

> [!example] 十六进制中的 PE 签名
> ```
> Offset      Content           Description
> 00000080    50 45 00 00       PE Signature = "PE\0\0"
> 00000084    4C 01             Machine = IMAGE_FILE_MACHINE_I386
> 00000086    04 00             NumberOfSections = 4
> ...
> ```
> 紧跟 `50 45 00 00` 之后的 `4C 01`、`04 00` 已经属于下一篇 [[05-详解PE文件(五)：COFF文件头|COFF 文件头]] 的 `Machine` 与 `NumberOfSections` 字段了。

## 编程验证 PE 签名

以下是一个简单的 C 语言示例，演示如何验证 PE 文件签名：

```c
#include <windows.h>
#include <stdio.h>

BOOL IsValidPEFile(LPCTSTR lpszFileName) {
    HANDLE hFile = CreateFile(lpszFileName, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return FALSE;
    }

    LPVOID lpBase = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!lpBase) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return FALSE;
    }

    // 检查DOS签名
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)lpBase;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        UnmapViewOfFile(lpBase);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return FALSE;
    }

    // 获取NT头位置
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)lpBase + dosHeader->e_lfanew);

    // 检查PE签名
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        UnmapViewOfFile(lpBase);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return FALSE;
    }

    UnmapViewOfFile(lpBase);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return TRUE;
}
```

代码逻辑正是上面流程图的实现：先验 `e_magic`（"MZ"），再用 `e_lfanew` 定位 NT 头，最后验 `Signature`（"PE\0\0"）。

## 相关常量定义

在 Windows SDK 中，以下常量用于表示不同的文件签名：

```c
#define IMAGE_DOS_SIGNATURE   0x5A4D      // "MZ"
#define IMAGE_OS2_SIGNATURE   0x454E      // "NE"
#define IMAGE_OS2_SIGNATURE_LE 0x454C     // "LE"
#define IMAGE_VXD_SIGNATURE   0x454C      // "LE"
#define IMAGE_NT_SIGNATURE    0x00004550  // "PE\0\0"
```

这些签名其实是一部"可执行格式进化史"：

| 签名 | 值 | 格式 |
|---|---|---|
| `MZ` | `0x5A4D` | DOS 可执行（最古老） |
| `NE` | `0x454E` | 16 位 Windows / OS2 |
| `LE` | `0x454C` | VxD 驱动 / OS2 |
| `PE` | `0x00004550` | 现代 32/64 位 Windows |

## 重要性

| 方面 | 说明 |
|---|---|
| **文件识别** | 操作系统通过该签名快速识别 PE 文件 |
| **格式验证** | 防止加载无效或损坏的 PE 文件 |
| **安全检查** | 分析工具通过验证签名判断文件类型 |
| **兼容性** | 确保文件符合 PE 格式规范 |

> [!summary] 小结
> PE 签名 `"PE\0\0"`（`0x00004550`）是 PE 格式的核心标识，作为 `IMAGE_NT_HEADERS` 的第一个成员，由 [[02-详解PE文件(二)：DOS头|e_lfanew]] 定位。它与开头的 "MZ" 构成"双门禁"：两道都通过，才进入 [[05-详解PE文件(五)：COFF文件头|COFF 文件头]] → [[06-详解PE文件(六)：可选头|可选头]] 的解析。验证 PE 签名是分析 Windows 可执行文件的基础技能。

---

← 上一篇：[[03-详解PE文件(三)：DOS Stub程序]]　｜　[[00-合集总览-PE文件详解|📚 返回合集总览]]　｜　下一篇：[[05-详解PE文件(五)：COFF文件头]] →
