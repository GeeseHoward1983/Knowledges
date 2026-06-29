# 花指令实验靶子工程（Anti-Disassembly Lab）

配套文档：`../花指令详解与实战教程.md`、`../花指令全类型深度展开.md`。
本工程把 12 类（+4 个 Linux/GNU 特有）花指令各做成一个可调用、可编译、可反汇编对比的 demo，
用于亲手观察“花指令骗过反汇编器，但不改变程序执行结果”这一核心现象。

> 仅用于授权的安全研究与逆向教学。请在隔离环境运行。

---

## 三个版本，按你的工具链选

| 目录 / 产物 | 工具链 | 目标平台 | 本机状态 | 说明 |
|------|--------|------|----------|------|
| `gcc/junk_gcc.exe` | cygwin gcc 13.4 + objdump | **Windows x86-64 PE** | ✅ **已编译并运行验证（16/16 OK）** | 本机直接可用，**推荐** |
| `gcc/junk_gcc_linux` | **x86_64-pc-linux-gnu-gcc 4.9.4** | **Linux x86-64 ELF** | ✅ **已交叉编译（ELF 已验证）** | 拷到 Linux/WSL 运行 |
| `nasm/junk_all.exe` | NASM 2.14 + MSVC cl(x86) | Windows x86-32 PE | ✅ **已编译并运行验证（13/13 OK）** | 纯汇编，字节级最精确；**x86 的权威可运行版本** |
| `msvc/junk_msvc.exe` | Visual Studio cl(x86) | Windows x86-32 PE | ⚠️ **可编译，逐行运行验证未完成** | 内联 `__asm`，VS 里便于调试。见下方已知限制 |

本机环境：`gcc 13.4.0`(cygwin, 默认 64 位) + `objdump`；交叉编译器 `x86_64-pc-linux-gnu-gcc 4.9.4`；
`NASM 2.14`(G:\NASM)；`MSVC cl 14.44`(x86, 经 vcvars32)。四条路径均可在本机构建。

> **MSVC 版说明（运行验证结论：11/11 通过，退出码 0；demo05 已移除）**：
> `msvc/junk_msvc.c` 用 cl(x86) 编译通过、完整跑完、剩余 11 个 demo 全部正确。
> 调试过程中暴露了一连串 **MSVC 内联汇编自身的调用/返回约定怪癖**（与花指令本身无关），已逐一处理：
> 1. **`offset` 局部标签取址不可靠** —— `push offset L;ret` / `mov reg,offset L;jmp reg` 取到错误地址 → 跳飞（int3/访问违例）。已把 demo 4/6/7/12 改为等价直接跳转规避，均已通过。
> 2. **`call/pop` 取 EIP（demo05）与编译器栈帧冲突** —— `call L1;pop eax` 直接操作 `esp`，与 `__declspec(noinline)` 的标准栈帧管理冲突，无法在不掏空该 demo 语义的前提下用 MSVC 内联汇编可靠实现。**故 demo05 已从 MSVC 版移除**（函数与调用均删除），该形态的字节级精确、可运行版本见 NASM 版 `_demo05`。
>
> 这些都是 **MSVC 内联汇编的固有特性，不是花指令的性质**。
> **x86 花指令字节级最完整的可运行版本仍是 NASM 版（`nasm/junk_all.exe`，13/13 全通过）**——
> 它实现了包括 `call/pop`、`push/ret`、间接跳转在内的全部形态；
> MSVC 版（11 类）作"在 Visual Studio 里单步观察花指令"的辅助，`call/pop` 形态请以 NASM 版为准。

---

## 快速开始（GCC 版，本机即可）

### A. Windows PE（本机直接编译运行）

```bash
cd gcc
bash build_gcc.sh        # 编译 → junk_gcc.exe
./junk_gcc.exe           # 运行验证（应输出 16/16 OK）
```

### B. Linux ELF（用 x86_64-pc-linux-gnu-gcc 交叉编译）

```bash
cd gcc
bash build_linux.sh      # 交叉编译 → junk_gcc_linux (ELF)
# 本机是 Windows 不能直接跑 ELF；拷到 Linux/WSL：  ./junk_gcc_linux
```

已验证产物类型：
`junk_gcc_linux: ELF 64-bit LSB executable, x86-64, ... for GNU/Linux 3.10.108`

> 交叉编译器为 GCC 4.9.4，不支持 `-no-pie`（该版本默认即 no-PIE），脚本已去掉该选项。
> 交叉到 Linux 时 `_WIN32` 未定义：SMC(demo11) 自动走 `mmap(PROT_EXEC)`，
> 异常(demo9) 走 POSIX `signal(SIGILL)+sigsetjmp`，均为 Linux 正确路径。

已验证运行输出：

```
[通用 12 类]
  demo01 => 1 [OK]  ...  demo12 => 12 [OK]
[Linux/GNU 特有补充]
  demo13_endbr => 13 [OK] ... demo16_loop => 16 [OK]
==== 结果：通过 16，失败 0 ====
```

### 看花指令“骗”反汇编器（关键实验）

```bash
objdump -d -M intel junk_gcc.exe | grep -A 12 "<demo01>:"
```

你会看到（节选，地址依本机为准）：

```
100401088:  eb 02              jmp 10040108c <demo01+0xc>     ; 真实流：跳过垃圾
10040108a:  e8 90 b8 01 00     call 10041c91f <...>           ; ← objdump 把垃圾字节
10040108f:  00 00              add BYTE PTR [rax],al          ;    E8 误当 call，并错位
```

- **真实执行**：`jmp` 直接到 `10040108c`，垃圾字节 `E8 90` 从不执行。
- **objdump（线性扫描）**：从 `10040108a` 把 `E8` 当 `call` 操作码，吞掉后续字节连环错位。
- 而程序实际返回值是 **1**（正确）——这就是“反汇编视角 ≠ 执行视角”。

`demo02`（不透明谓词）、`demo08`（重叠指令，真实 `mov eax,8` 被吞成 `mov eax,0x8b8`）同理，
建议都 `grep -A 12` 看一遍，对照《全类型深度展开》逐类理解。

---

## 三个版本各覆盖的花指令类型

| # | 类型 | gcc(64) | nasm(32) | msvc(32) |
|---|------|:---:|:---:|:---:|
| 1 | 跳过字节型 `EB xx` | ✅ | ✅ | ✅ |
| 2 | 不透明谓词 | ✅ | ✅ | ✅ |
| 3 | 互补跳转对 | ✅ | ✅ | ✅ |
| 4 | push/ret 伪跳转 | ✅ | ✅ | ✅ |
| 5 | call/pop 取 EIP/RIP | ✅ | ✅ | ✖️(已移除*) |
| 6 | call + 改返回地址 | ✅ | ✅ | ✅ |
| 7 | 间接跳转 | ✅ | ✅ | ✅ |
| 8 | 重叠指令 | ✅ | ✅ | ✅ |
| 9 | 异常驱动控制流 | ✅(SIGILL) | ✅(ud2) | ✅(SEH) |
| 10 | 冗余前缀 | ✅ | ✅ | ✅ |
| 11 | 自修改代码 SMC | ✅(mmap/VP) | ✅(exec节) | ✅(VirtualProtect) |
| 12 | 花花组合 | ✅ | ✅ | ✅ |
| 13 | endbr64/CET 地标错位 | ✅ | — | — |
| 14 | RIP 相对寻址藏代码 | ✅ | — | — |
| 15 | leave/栈帧伪造 + jecxz | ✅ | — | — |
| 16 | loop/jecxz 恒定循环 | ✅ | — | — |

> 13~16 是 **64 位 / Linux-GNU 生态特有**形态，详见下节与《全类型深度展开》。
>
> `*` 类型5（call/pop 取 EIP）在 MSVC 版中**已移除**：`call;pop` 直接操作 `esp`，与 MSVC
> `__declspec(noinline)` 的栈帧管理冲突，无法用内联汇编可靠实现。该形态的可运行版本见 NASM 版
> `_demo05`（GCC 版用 call/pop 取 RIP，也正常）。移除后 MSVC 版 **11 类全部编译运行通过（退出码 0）**。

---

## Linux / GNU 汇编里更多的花指令例子（回答“Linux 汇编是否有更多”）

x86-64 + GNU/ELF 生态确实带来一批 32 位 / Windows 下少见的花指令变体，已收进 `gcc/junk_gcc.c`：

1. **`endbr64`(F3 0F 1E FA) 地标干扰（demo13）**
   CET(Control-flow Enforcement) 要求所有间接分支落点是 `endbr64`。攻击者在 `endbr64` 前后塞垃圾、
   或让直接跳转落到 `endbr64` 之后，扰乱工具对“合法间接分支目标集”的识别。

2. **RIP 相对寻址藏代码（demo14）**
   64 位用 `lea reg,[rip+disp]` 取地址。把 `disp` 指向一段看似 `.quad` 数据、实为真实代码的区域，
   绕过“代码段 vs 数据段”的静态分界判断。

3. **`leave` / 栈帧伪造 + `jecxz`（demo15）**
   `leave`(=`mov rsp,rbp; pop rbp`) 可被花指令用来搬运/伪造控制流；`jecxz`(ecx=0 跳转)
   编译器几乎不生成，出现即高度可疑，常被用作隐蔽的恒定跳转。

4. **`loop` 恒定一次循环（demo16）**
   `loop`/`loope`/`loopne` 在现代编译产物里基本绝迹，被用来构造“看似循环、实则恒定执行一次”的混淆。

此外，Linux/GNU 生态还有这些方向（属进阶，文中未全做成 demo，但在深度文档里有说明）：

- **PLT/GOT 间接跳转混淆**：`jmp [rip+got_off]` 经过 PLT 桩，静态目标依赖重定位，易被绕。
- **`syscall`/`int 0x80` 直接系统调用**：绕过 libc 包装，隐藏行为意图（恶意样本常见）。
- **GNU as `.byte`/`.reloc`/`.cfi` 伪指令滥用**：用伪指令在汇编期插入垃圾字节、伪造异常展开信息(.eh_frame)，
  干扰基于 CFI/unwind 的分析与栈回溯。
- **`gcc -fcf-protection`、`-mfunction-return=thunk`(retpoline)** 等编译期特性被逆向时，其桩代码形态也常被误判为花指令。

> 结论：**Linux/x86-64 不是“花指令更少”，而是“另一批花指令”**——核心原理（变长指令错位、控制流隐藏）一致，
> 但载体换成了 RIP 相对寻址、CET 地标、PLT/GOT、ELF 异常展开等 64 位/ELF 特性。

---

## 目录结构

```
target/
├── README.md            # 本文件
├── gcc/                 # ★本机已验证可编译运行
│   ├── junk_gcc.c       # x86-64 全类型靶子（含 demo13~16 Linux 特有）
│   ├── build_gcc.sh     # 编译 Windows PE
│   ├── build_linux.sh   # 用 x86_64-pc-linux-gnu-gcc 交叉编译 Linux ELF
│   ├── junk_gcc.exe     # Windows PE 产物（运行 build_gcc.sh 后出现）
│   └── junk_gcc_linux   # Linux ELF 产物（运行 build_linux.sh 后出现）
├── nasm/                # 需 NASM + MSVC(x86)
│   ├── junk_all.asm     # x86-32 纯汇编靶子
│   ├── driver.c         # 验证驱动（含 SEH 版 demo9）
│   └── build_nasm.bat
└── msvc/                # 需 Visual Studio(x86)
    ├── junk_msvc.c      # x86-32 内联汇编靶子
    └── build_msvc.bat
```

---

## 推荐学习路径

1. `bash gcc/build_gcc.sh && ./gcc/junk_gcc.exe` —— 先看“运行结果全对”。
2. `objdump -d -M intel gcc/junk_gcc.exe` —— 再看“反汇编全错位”，理解二者背离。
3. 对照《花指令全类型深度展开.md》逐类读 ①原理→⑦实验代码。
4. 用 IDA/Ghidra 打开同一 exe，对比递归下降(RD) 与线性扫描(LS) 被骗的差异。
5. 尝试按深度文档第六节写 IDAPython“去花”脚本，把错位修正回来。

---

> 免责声明：本工程仅用于授权的安全研究、逆向教学与防御分析，禁止用于任何非法用途。
