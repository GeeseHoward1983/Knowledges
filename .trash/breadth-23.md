# 选题广度审查 · 系列 23 C/C++ 运行时与 ABI

> 审查基准：以「C/C++ 运行时与 ABI」领域完整知识地图为坐标，对照现有 14 篇，找出尚未独立成篇、但真正有独立成篇价值的主题。
> 现有覆盖：启动链（crt0/init_array/静态初始化）、C++ ABI（mangling/布局/vtable/多重继承/RTTI）、异常展开（ABI/eh_frame/DWARF CFI）、调用规约（SysV AMD64/Win64 对照）、TLS、运行时库实战。

---

### 拟新增：Windows x64 SEH 与 Unwind ABI 专章

Windows x64 结构化异常处理（SEH）完全独立于 Itanium ABI，采用**基于 .pdata/.xdata 的 RUNTIME_FUNCTION 表**和微软私有 unwind code 格式，与 Linux .eh_frame/DWARF CFI 路线截然不同。当前第 10 章仅在对照栏一笔带过，完全没有展开 .pdata 节结构、unwind code 编码、`RtlVirtualUnwind` 流程、__try/__except/__finally 的 scope table、以及 x64 上 EH 如何通过 unwind info 实现而非 fs:[0] 链表。
这是逆向 Windows PE（尤其驱动/shellcode）时每天都要面对的 ABI 契约，与系列 38 Windows 内核逆向高度互补。

**拟涵盖要点**：.pdata/.xdata 节结构、RUNTIME_FUNCTION 与 UNWIND_INFO 编码、unwind code 操作码表、epilog 识别规则、C++ EH 在 Win64 上的 scope table 实现、`RtlLookupFunctionEntry`/`RtlVirtualUnwind` 调用链、与 Linux DWARF CFI 的对比。

---

### 拟新增：C++ 协程 ABI 与协程帧布局

C++20 协程（`co_await`/`co_yield`/`co_return`）在编译器层面被变换为**协程帧（coroutine frame）**——一个堆分配的状态机对象，包含 promise、参数副本、局部变量快照、恢复/销毁函数指针。这套变换是 ABI 级契约，决定了帧的内存布局（resume pointer 在 offset 0、destroy pointer 在 offset 8）、`coroutine_handle<>` 的本质（裸指针）、以及调用 `.resume()` 时如何跳转到内部 switch-label。
现有 14 篇完全未涉及协程，而逆向 C++20 二进制时协程帧与普通 vtable/异常帧混在一起，不了解其 ABI 就看不懂反汇编。

**拟涵盖要点**：编译器如何将协程函数 lowering 为状态机、协程帧 struct 布局（promise/index/locals）、resume/destroy 函数指针偏移（ABI 固定）、`coroutine_handle` 与帧指针的关系、对称传递（symmetric transfer）的尾调用 ABI、协程与异常/析构的交互。

---

### 拟新增：Sanitizer 运行时 ABI（ASan / UBSan / TSan）

ASan、UBSan、TSan 是通过**编译器插桩 + 运行时库**实现的，它们本身构成一层运行时 ABI：shadow memory 映射规则（ASan 的 `addr >> 3` shadow 地址计算）、插桩代码如何在每次访存前注入 `__asan_load*`/`__asan_store*` 调用、UBSan 如何插入 `__ubsan_handle_*` 回调、TSan 如何使用影子词（shadow word）跟踪并发访问。对逆向/漏洞研究者而言，ASan 插桩二进制是分析内存错误的重要手段，理解其 ABI 才能：一、在逆向时识别并剔除 sanitizer 插桩噪声；二、在 fuzzing 流程中正确解读 crash 报告；三、绕过或利用 sanitizer 的检测盲区。现有 14 篇无任何 sanitizer 内容。

**拟涵盖要点**：ASan shadow memory 布局与地址映射公式、`__asan_load{1,2,4,8}`/`__asan_store*` 插桩点选取规则、redzones 与 quarantine 机制、UBSan handler 命名约定与 `__ubsan_handle_*` 调用 ABI、TSan shadow word 格式、`__tsan_read*`/`__tsan_write*` 插桩、在逆向中识别 sanitizer 代码的技巧。

---

### 拟新增：std::function / std::any 类型擦除的 ABI 与实现剖析

`std::function`、`std::any`（C++17）、`std::move_only_function`（C++23）是标准库中最典型的**类型擦除**实现，其底层使用 small-buffer optimization（SBO）+ 虚函数表（invoker table）模式，在二进制层面产生特定的布局。逆向 C++ 二进制时，`std::function` 对象随处可见（回调、信号槽、异步任务），但其内部 invoker 指针和 SBO 缓冲区的布局取决于标准库实现（libstdc++/libc++/MSVC STL），不同实现之间 ABI 不兼容。理解这套机制，才能在反汇编中识别 `std::function::operator()` 的虚调用、定位实际回调函数。现有篇目仅覆盖 vtable/RTTI，未涉及 type erasure 的 ABI 细节。

**拟涵盖要点**：SBO 阈值与布局（libstdc++ 16 字节、libc++ 24 字节）、invoker/manager 函数指针表结构、`_M_invoker`/`_M_manager` ABI、`std::any` 的 handler 函数 ABI、MSVC STL 差异、在反汇编中还原被擦除类型的方法。

---

### 拟新增：LTO / IPO 与内联对 ABI 边界的影响

链接时优化（LTO）和过程间优化（IPO）可以跨编译单元内联函数、消除符号、修改调用约定，从而产生**在单一目标文件层面看不到、只有在最终二进制中才能观察到的 ABI 变化**：函数可能消失于符号表（内联掉了）、参数传递方式可能被优化（常量传播后参数被消除）、ThinLTO 的 module summary 机制引入新的中间格式。对逆向者来说，LTO 编译的二进制缺少符号、函数边界模糊，是逆向难度的重要来源之一。现有篇目完全未涉及 LTO 对 ABI 的影响。

**拟涵盖要点**：LTO/ThinLTO 的编译器内部表示（LLVM IR bitcode / GCC GIMPLE）、LTO 如何修改调用规约（internalize/devirtualize/inline）、对符号可见性与 ABI 边界的影响、逆向 LTO 编译二进制的识别技巧（fat object、.llvmbc 节）、何处 ABI 契约仍然保留（exported 符号）、调试信息在 LTO 下的变化。

---

### 拟新增：栈保护运行时（Stack Canary / Shadow Stack / SafeStack）

GCC/Clang 的 `-fstack-protector`、Intel CET 的 Shadow Stack、LLVM SafeStack 都在运行时层引入了额外的**栈完整性契约**：canary 值从 TLS（`fs:0x28`）读取并在函数入口/出口校验，Shadow Stack 在硬件层维护独立的返回地址栈。这些机制本身构成运行时 ABI 的一部分——逆向时必须识别 canary 检查代码（`mov rax, fs:0x28` → `xor rax, [rsp+N]` → `jne __stack_chk_fail`），漏洞利用时必须绕过或伪造 canary，固件分析时必须判断目标是否开启了 CET。现有篇目涵盖 TLS（第 13 章知道 `fs:0x28` 是 canary 存储位置），但完全没有专章讲解这套运行时保护机制。

**拟涵盖要点**：`__stack_chk_guard` 在 TLS 中的位置与初始化时机、`-fstack-protector`/`-strong`/`-all` 的插桩策略差异、canary 在汇编层的读写模式（识别技巧）、Intel CET Shadow Stack（`WRSS`/`INCSSPQ`）工作原理、LLVM SafeStack 的安全栈/不安全栈划分、绕过技术（leak-then-overwrite）与防御的 ABI 层响应。

---

### 拟新增：C++20 Modules 对 ABI 与名字查找的影响

C++20 modules（`.ixx` / `import std`）改变了编译单元边界和名字导出机制，对 ABI 产生深层影响：模块接口单元（module interface unit）的 BMI（Binary Module Interface）格式、导出符号的 mangling 方案（GCC/Clang 在 Itanium mangling 中加入模块名限定符）、ODR（One Definition Rule）在 module 边界的语义变化、以及 ABI 兼容性问题（不同编译器的 BMI 格式不兼容导致 modules 暂时无法跨编译器 ABI 使用）。随着 C++20/23 在生产代码中的普及，逆向 modules 编译的二进制将成为现实需求。现有 14 篇无任何 modules 内容。

**拟涵盖要点**：module 编译模型（BMI/CMI 格式）、导出符号的 Itanium mangling 扩展（`W` 前缀模块名编码）、GCC/Clang/MSVC 三家 BMI 格式差异、ODR 语义变化对链接器的影响、modules 下的 inline 函数与模板实例化 ABI、逆向视角：如何识别 modules 编译产物。

---

### 拟新增：Rust / Swift 与 C++ 的 ABI 互操作

Rust（`extern "C"`/`#[repr(C)]`）和 Swift（`@_cdecl`/C interop）都提供与 C ABI 互操作的机制，但它们各自也有**私有 ABI**（Rust 的默认调用规约未稳定、Swift 有自己的 ownership/existential ABI）。在混合语言二进制（iOS App = Swift + ObjC + C++、游戏引擎 = Rust + C++）中，理解 FFI 边界处的 ABI 转换点至关重要。Swift 还有一套**稳定的 ABI**（Swift 5.0 起），其 existential container、witness table、metadata 机制与 C++ vtable/RTTI 对应但不同，逆向 iOS/macOS 二进制时两套 ABI 交织出现。现有篇目无任何跨语言 ABI 互操作内容。

**拟涵盖要点**：Rust `extern "C"` 与 `#[repr(C)]` 的 ABI 保证边界、Rust 私有 ABI（未稳定）的现状、Swift ABI 稳定性保证（Swift 5+）、Swift existential container 布局（value buffer + witness table + metadata）、Swift witness table vs C++ vtable 对比、`@_cdecl` 与 C++ interop、混合二进制逆向：识别 Rust/Swift 与 C++ 边界。

---

## 汇总

| 序号 | 拟标题 | 独立成篇理由 |
|------|--------|-------------|
| A | Windows x64 SEH 与 Unwind ABI | .pdata/.xdata 是 Windows PE 逆向的核心契约，与 Linux DWARF 路线完全不同，内容量大 |
| B | C++ 协程 ABI 与协程帧布局 | C++20 编译器变换产生特定帧布局，逆向必须理解，现有篇完全未涉及 |
| C | Sanitizer 运行时 ABI（ASan/UBSan/TSan） | 插桩模式与 shadow memory ABI 是逆向/fuzzing 工作流的基础知识 |
| D | std::function / 类型擦除的 ABI | 标准库最常见的 ABI 隐患，SBO 布局跨实现不兼容，逆向识别价值高 |
| E | LTO/内联对 ABI 边界的影响 | LTO 编译二进制是逆向难点来源，专章才能讲清楚符号消失与边界模糊机制 |
| F | 栈保护运行时（Canary / Shadow Stack） | `fs:0x28` canary、CET、SafeStack 均有运行时 ABI 细节，漏洞利用必须掌握 |
| G | C++20 Modules 对 ABI 的影响 | mangling 扩展、BMI 格式、ODR 变化是未来逆向 C++20 二进制的必备知识 |
| H | Rust / Swift 与 C++ ABI 互操作 | 混合语言二进制（iOS/游戏引擎）日益普遍，FFI 边界与 Swift ABI 独立成章才够深 |
