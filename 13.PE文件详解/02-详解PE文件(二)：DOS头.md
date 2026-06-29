---
title: "详解PE文件(二)：DOS头"
series: PE文件详解
order: 2
source: https://blog.csdn.net/geesehoward20000/article/details/154741971
author: geesehoward20000（阿捏利）
published: 2025-11-13
collected: 2026-06-26
aliases:
  - DOS头
  - IMAGE_DOS_HEADER
  - PE(二)
summary: PE 文件的第一个结构 IMAGE_DOS_HEADER（固定 64 字节）。真正关键的只有两个字段——e_magic（"MZ"）做文件类型签名，e_lfanew 指向真正的 PE 头。
tags:
  - PE文件
  - Windows
  - 逆向工程
  - 二进制文件格式
  - PE头部结构
---
# 详解PE文件(二)：DOS头

> 来源：[CSDN 原文](https://blog.csdn.net/geesehoward20000/article/details/154741971)　｜　发布：2025-11-13　｜　合集《PE文件详解》第 2/26 篇

> [!tip] 一句话核心
> DOS 头固定 **64 字节**，但现代 PE 真正在意的只有一头一尾两个字段：开头的 **`e_magic`（"MZ"）** 用来"认门"，末尾的 **`e_lfanew`** 是一根指针，告诉加载器"真正的 PE 头在文件的哪个偏移"。中间那一堆字段都是 DOS 时代的遗产，今天基本只是占位。

> [!info] 速查
> - **结构体**：`IMAGE_DOS_HEADER`
> - **大小**：固定 64 字节（`0x40`），32/64 位一致
> - **必查字段**：`e_magic`（偏移 `0x00`，2 字节）、`e_lfanew`（偏移 `0x3C`，4 字节）
> - **位置**：文件最开头，后接 [[03-详解PE文件(三)：DOS Stub程序|DOS Stub]]

每个 PE 文件（[[01-详解PE文件(一)：PE文件基础概念|.exe/.dll/.sys]]）都从 DOS 头开始。它的全称是 **DOS Header**，对应结构体 `IMAGE_DOS_HEADER`，存在的唯一理由是**向后兼容**：让这个文件即使被丢到古老的 DOS 系统里双击，也能"体面地"给出一句提示而不是直接崩溃。现代 Windows 早已不跑在 DOS 上，但这块历史遗留至今仍钉在每个 PE 文件的最前面。

## DOS 头在 PE 文件中的位置

理解 DOS 头，关键是看清它和后面结构的关系——它本质上是一座**从 "MZ" 通往 "PE" 的桥**：

```text
文件偏移
0x00  ┌────────────────────────────┐
      │  DOS 头 (IMAGE_DOS_HEADER)  │  固定 64 字节
      │    e_magic = "MZ" ─────────┼─ 认门：是不是可执行文件
      │    ......                   │
0x3C  │    e_lfanew ───────────┐    │  指针：PE 头在哪
0x40  ├────────────────────────┼────┤
      │  DOS Stub 程序          │    │  "This program cannot be run in DOS mode"
      ├────────────────────────┼────┤
0x?? ◄┘  PE 签名 "PE\0\0"  ◄───┘    │  ← e_lfanew 指向这里（见第四篇）
      ├────────────────────────────┤
      │  COFF 文件头 / 可选头 / ... │
      └────────────────────────────┘
```

加载器读文件的前两步永远是：① 看开头是不是 `MZ`；② 跳到偏移 `0x3C` 取出 `e_lfanew`，按它指向的位置去找 PE 头。中间的 [[03-详解PE文件(三)：DOS Stub程序|DOS Stub]] 会被直接跳过。

## 结构定义：IMAGE_DOS_HEADER

DOS 头大小**固定为 64 字节（0x40）**，32 位和 64 位 PE 都一样。完整结构如下：

```c
typedef struct _IMAGE_DOS_HEADER {
    WORD e_magic;         // 魔数，标识DOS可执行文件，固定值为"MZ"(0x5A4D)
    WORD e_cblp;          // 文件最后一页的字节数
    WORD e_cp;            // 文件页数
    WORD e_crlc;          // 重定位项数
    WORD e_cparhdr;       // 头部大小（以段为单位）
    WORD e_minalloc;      // 所需最小额外段
    WORD e_maxalloc;      // 所需最大额外段
    WORD e_ss;            // 初始SS值（相对）
    WORD e_sp;            // 初始SP值
    WORD e_csum;          // 校验和
    WORD e_ip;            // 初始IP值
    WORD e_cs;            // 初始CS值（相对）
    WORD e_lfarlc;        // 重定位表的文件地址
    WORD e_ovno;          // 覆盖号
    WORD e_res[4];        // 保留字
    WORD e_oemid;         // OEM标识符
    WORD e_oeminfo;       // OEM信息
    WORD e_res2[10];      // 保留字
    LONG e_lfanew;        // 新exe头的文件地址（PE头的偏移量）
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;
```

把 64 字节按偏移摊开看，能更直观地分清"必看"与"可略"：

| 偏移 | 字段 | 大小 | 说明 |
|---|---|---|---|
| `0x00` ⭐ | `e_magic` | 2 | **"MZ" 签名，必查** |
| `0x02`~`0x1B` | `e_cblp` … `e_ovno` | 26 | DOS 时代的页/重定位/寄存器参数，现代忽略 |
| `0x1C`~`0x23` | `e_res[4]` | 8 | 保留 |
| `0x24`~`0x27` | `e_oemid` / `e_oeminfo` | 4 | OEM 标识 |
| `0x28`~`0x3B` | `e_res2[10]` | 20 | 保留 |
| `0x3C` ⭐ | `e_lfanew` | 4 | **指向 PE 头的偏移，必读** |

> [!note] 为什么是 "MZ"？
> `e_magic` 的两个字节是 `4D 5A`，正是 ASCII 的 **"MZ"**——这是 DOS 可执行格式设计者 **Mark Zbikowski** 的姓名缩写。三十多年过去，他的名字仍刻在世界上每一个 Windows 程序的头两个字节里。

## 两个关键字段

整张表里，今天还真正起作用的只有这两个，建议重点记住。

### e_magic — 文件类型签名（偏移 0x00，2 字节）

- 位于文件最开头，固定值 `0x5A4D`，即 ASCII 字符 **"MZ"**。
- 加载器拿到文件第一件事就是查它：**不是 "MZ"，直接判定为非可执行文件**，后面都不用看了。
- 它只能证明"这是个 DOS 可执行体"，能否当 PE 跑还要靠 `e_lfanew` 找到的 PE 签名二次确认（见 [[04-详解PE文件(四)：PE文件签名]]）。

### e_lfanew — 通往 PE 头的指针（偏移 0x3C，4 字节）

这是 DOS 头里**唯一与 PE 格式强相关**的字段，也是整个解析流程的"路标"：

- 类型为 `LONG`，是一个**从文件头算起的绝对偏移量**，指向 PE 签名 `"PE\0\0"` 的位置。
- Windows 加载器靠它定位 PE 头——所以解析 PE 的标准写法就是：读 `0x3C` 处 4 字节得到 `e_lfanew`，再 `seek` 过去。
- 它的值**不固定**，取决于 DOS Stub 的长度，常见值有 `0x80`、`0xE8`、`0xF8` 等。"DOS 头 64 字节 + DOS Stub 一段" 加起来就是 `e_lfanew`。

> [!warning] 逆向 / 安全提醒
> `e_lfanew` 是手工解析与恶意软件常做手脚的地方：把它指向异常位置、或在 DOS 头与 PE 头之间塞入额外数据，是常见的免杀 / 反分析技巧。解析器若不校验它的合法范围，很容易被引导去读到错误的结构。

## 其余字段：DOS 时代的遗产

除上面两个外，其它字段都是为 DOS 环境准备的，现代 Windows 加载 PE 时一律忽略，但了解它们有助于读懂十六进制：

- `e_cblp`、`e_cp`、`e_crlc` 等：DOS 下的内存管理与程序加载参数。
- `e_ss`、`e_sp`、`e_cs`、`e_ip`：DOS 下的初始寄存器（栈段/栈指针/代码段/指令指针）值。
- `e_res`、`e_res2`：保留字段，留作将来扩展，实际填 0。

## 实战：用十六进制看 DOS 头

随便拿一个 EXE 用十六进制工具打开，开头一般长这样（关注三处加注释的字节）：

> [!example] 十六进制解析演示
> ```text
> 偏移        字节                          含义
> 00000000    4D 5A 90 00 03 00 00 00       e_magic = "MZ"(4D 5A), e_cblp=0x0090, e_cp=0x0003
>             ......
> 0000003C    F8 00 00 00                   e_lfanew = 0x000000F8  → PE 头在文件偏移 0xF8
>             ......
> 000000F8    50 45 00 00                   "PE\0\0"  ← e_lfanew 指向此处（PE 签名）
> ```

加载器据此判定一个文件是否为合法 PE，流程可以画成：

```text
   读偏移 0x00（2 字节）
          │
          ▼
     == "MZ" ?  ──否──▶  不是可执行文件，拒绝加载
          │ 是
          ▼
   读偏移 0x3C（4 字节） ──▶ 得到 e_lfanew
          │
          ▼
   seek 到 e_lfanew 处
          │
          ▼
    == "PE\0\0" ?  ──否──▶  不是合法 PE
          │ 是
          ▼
   继续解析 COFF 文件头  ─▶ 见 [05]
```

对应代码里就是这三步：

1. 读偏移 `0x00` 两字节 = `4D 5A` → 是 "MZ"，校验通过。
2. 读偏移 `0x3C` 四字节 = `F8 00 00 00`（小端）→ `e_lfanew = 0xF8`。
3. 跳到 `0xF8`，读到 `50 45 00 00` = `"PE\0\0"` → 确认是合法 PE，继续解析后面的 [[05-详解PE文件(五)：COFF文件头|COFF 文件头]]。

> [!summary] 小结
> DOS 头是 PE 文件的历史遗留，却仍是解析的起点。**记住一头一尾**：`e_magic`（"MZ"）负责"认门"，`e_lfanew` 负责"指路"，二者一前一后框定了从 DOS 世界到 PE 世界的入口。中间的 64 字节其余内容在现代系统里基本只是占位。理解这一结构，是后续分析 [[03-详解PE文件(三)：DOS Stub程序|DOS Stub]]、[[04-详解PE文件(四)：PE文件签名|PE 签名]] 乃至整个 PE 格式、做逆向与恶意代码分析的第一步。

---

← 上一篇：[[01-详解PE文件(一)：PE文件基础概念]]　｜　[[00-合集总览-PE文件详解|📚 返回合集总览]]　｜　下一篇：[[03-详解PE文件(三)：DOS Stub程序]] →
