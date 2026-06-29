---
title: "详解PE文件(三)：DOS Stub程序"
series: PE文件详解
order: 3
source: https://blog.csdn.net/geesehoward20000/article/details/154742133
author: geesehoward20000（阿捏利）
published: 2025-11-14
collected: 2026-06-26
aliases:
  - DOS Stub
  - DOS Stub程序
  - PE(三)
summary: 位于 DOS 头与 PE 头之间的一段 16 位 DOS 小程序，作用是在 DOS 环境下打印"This program cannot be run in DOS mode."。现代系统靠 e_lfanew 直接跳过它。
tags:
  - PE文件
  - Windows
  - 逆向工程
  - 二进制文件格式
  - PE头部结构
---
# 详解PE文件(三)：DOS Stub程序

> 来源：[CSDN 原文](https://blog.csdn.net/geesehoward20000/article/details/154742133)　｜　发布：2025-11-14　｜　合集《PE文件详解》第 3/26 篇

> [!tip] 一句话核心
> DOS Stub 是夹在 [[02-详解PE文件(二)：DOS头|DOS 头]] 与 [[04-详解PE文件(四)：PE文件签名|PE 头]] 之间的一段**真正可运行的 16 位 DOS 小程序**。它存在的唯一目的：当有人在古老的 DOS 上执行这个文件时，打印一句"This program cannot be run in DOS mode."而不是让机器乱跑。现代 Windows 加载器靠 `e_lfanew` **直接跳过它**。

> [!info] 速查
> - **位置**：DOS 头（64B）之后、PE 签名之前
> - **大小**：可变，通常几十～几百字节
> - **本质**：一个合法的 16 位 DOS EXE（有自己的程序头 + 代码 + 数据）
> - **典型输出**：`This program cannot be run in DOS mode.`
> - **现代命运**：被 [[02-详解PE文件(二)：DOS头|e_lfanew]] 跳过，几乎不执行

DOS Stub 程序位于 DOS 头之后、PE 头之前，是一段小型 DOS 程序，用于在不认识 PE 格式的环境里给出有意义的反馈。它是微软"向后兼容"哲学的一个缩影——为一个几乎不会发生的场景（在 DOS 上跑 Windows 程序）保留了一段真实代码。

## 位置与作用

它在文件里的位置正好卡在"认门"和"正片"之间：

```text
0x00  ┌──────────────────────┐
      │  DOS 头 (64B)         │
0x3C  │   e_lfanew ───────┐   │
0x40  ├───────────────────┼───┤
      │  DOS Stub  ◄ 我们在这 │   │  ← 在 DOS 下被执行：打印提示后退出
      ├───────────────────┼───┤
0x?? ◄┘  PE 签名 "PE\0\0" ◄┘   │  ← 现代 Windows 直接跳到这里
      └──────────────────────┘
```

它的核心作用是：在 **MS-DOS 等不支持 PE 的系统**上执行该文件时，运行这段 Stub 而不是去执行后面的 Windows 代码，从而向用户给出清晰提示，避免崩溃或乱码。

## DOS Stub 程序的结构

DOS Stub 本身是一个有效的 16 位 DOS 可执行程序，通常包含以下部分：

1. **DOS 程序头**：标准的 DOS EXE 头结构
2. **程序代码**：实际的 DOS 程序指令
3. **数据**：程序使用的数据，如要显示的错误信息字符串

## 典型的 DOS Stub 程序

大多数编译器和链接器生成的 PE 文件都包含一个标准的 DOS Stub 程序，它会显示如下信息：

```
This program cannot be run in DOS mode.
```

或者类似的信息，告知用户该程序需要 Windows 环境才能运行。

> [!example] 典型 DOS Stub 的汇编实现
> ```asm
> ; 典型的DOS Stub程序
> mov ah, 09h              ; DOS功能号09h - 显示字符串
> mov dx, offset message   ; 消息字符串的偏移地址
> int 21h                  ; 调用DOS中断
> mov ax, 4C01h            ; DOS功能号4Ch - 终止程序
> int 21h                  ; 调用DOS中断
>
> message db 'This program cannot be run in DOS mode.$'
> ```
> 逻辑很朴素：用 DOS 中断 `int 21h` 的 `09h` 功能打印以 `$` 结尾的字符串，再用 `4Ch` 功能退出。

## DOS Stub 程序的位置和大小

DOS Stub 程序紧跟在 `IMAGE_DOS_HEADER` 之后，其大小是可变的。在标准的 PE 文件中，DOS Stub 程序的大小通常在几十到几百字节之间。PE 头的实际位置可以通过 DOS 头中的 `e_lfanew` 字段找到。

文件结构如下：

```
+---------------------+
|   DOS Header        |  <- 固定64字节
+---------------------+
|   DOS Stub Program  |  <- 可变大小
+---------------------+
|   PE Header         |  <- 由e_lfanew指向
+---------------------+
|   Section Table     |
+---------------------+
|   Section Data      |
+---------------------+
```

> [!note] 为什么 e_lfanew 不固定
> 正因为 DOS Stub 大小可变，`e_lfanew`（= 64 字节 DOS 头 + Stub 长度）才不是固定值。改变 Stub 长度，PE 头的偏移就跟着变——这也是 [[02-详解PE文件(二)：DOS头|上一篇]] 强调"`e_lfanew` 值不固定"的根本原因。

## 自定义 DOS Stub 程序

程序员可以根据需要创建自定义的 DOS Stub 程序。例如，可以通过链接器选项指定自定义的 Stub 程序：

```
link /stub:my_stub.exe program.obj
```

这允许开发者显示自定义的错误信息，或者甚至在 DOS 环境下执行一些有用的功能。

## 实际应用中的意义

| 价值 | 说明 |
|---|---|
| **兼容性** | 确保 PE 文件在 DOS 环境下不会产生不可预测的行为 |
| **用户体验** | 提供清晰的错误信息，而不是崩溃或乱码 |
| **品牌识别** | 一些公司会定制 Stub，显示厂商标识或版权信息 |

## 现代意义与逆向价值

在现代 Windows 系统中，DOS Stub 程序很少被实际执行，因为系统直接跳转到 PE 头开始解析和加载程序。然而，它仍然是 PE 文件格式的重要组成部分，体现了微软对向后兼容性的重视。

> [!warning] 逆向视角：别忽略这块"废弃"区域
> 对逆向工程师和安全分析人员来说，DOS Stub 有时藏着有价值的线索：编译器版本、开发工具信息等都可能留痕于此。同时，这块"通常没人看"的区域也常被用来隐藏数据或塞入额外内容——分析时不应假定它一定是标准模板。

总的来说，DOS Stub 程序虽然简单，但它是连接旧时代 DOS 系统和现代 Windows 系统的重要桥梁，体现了计算机技术发展的历史延续性。

> [!summary] 小结
> DOS Stub 是 DOS 头与 [[04-详解PE文件(四)：PE文件签名|PE 签名]] 之间一段可变长度的 16 位 DOS 程序，典型行为是打印"This program cannot be run in DOS mode."。它由 [[02-详解PE文件(二)：DOS头|e_lfanew]] 跳过、现代几乎不执行，却因此成为体积可变、易藏信息的一块区域，在逆向与取证中值得多看一眼。

---

← 上一篇：[[02-详解PE文件(二)：DOS头]]　｜　[[00-合集总览-PE文件详解|📚 返回合集总览]]　｜　下一篇：[[04-详解PE文件(四)：PE文件签名]] →
