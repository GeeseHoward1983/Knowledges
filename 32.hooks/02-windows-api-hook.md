# 02 · Windows API Hook / 函数 Hook

> 本篇覆盖 Windows 平台下从**消息钩子**到**Inline Hook**的全套技术：`SetWindowsHookEx` 14 种钩子、IAT/EAT Hook、Inline Hook（含 trampoline、热补丁、x64 跳转编码）、Detours/MinHook 的工作原理与完整可编译示例、VEH Hook、DLL 注入的 4 种方式、PatchGuard 与 PG 绕过简述、调试与卸载策略。
> 假设读者：会用 C/C++，熟悉 Windows PE 格式基础（Section、Import Table 大致结构），有 Win32 编程经验。

---

## 1. 概念地图

Windows 上"hook"是一个被严重重载的词，至少指 5 种完全不同的技术：

| 名称 | 工作层 | 谁提供 | 典型用途 |
|---|---|---|---|
| **Windows Hook API**（`SetWindowsHookEx`） | OS 提供的消息分发拦截 | Win32 子系统 | 全局键盘/鼠标钩子、CBT、WH_GETMESSAGE |
| **IAT / EAT Hook** | PE 导入/导出表替换 | 自实现 | 拦截 DLL 调用、监控 API 使用 |
| **Inline Hook** | 机器码改写 | 自实现 / Detours / MinHook | 拦截任意函数（不论是否从导入表调用） |
| **VEH Hook**（Vectored Exception Handler） | 借异常分发 | OS + 自实现 | 软件断点式 hook、反调试 |
| **API Sets / Detours Shim** | 微软 App Compat | OS | 系统级 API 重定向，应用层一般不用 |

每种技术解决的问题、限制、卸载方式都不同。**选错技术是 Windows hook 项目最常见的失败原因。**

---

## 2. SetWindowsHookEx —— 系统级消息钩子

### 2.1 工作原理

`SetWindowsHookEx(idHook, lpfn, hMod, dwThreadId)` 是 Win32 提供的"消息分发链注入"机制。Windows 在派发某些消息到目标窗口前，会**遍历钩子链**，依次调用每个注册的回调；任何回调可以选择继续调用 `CallNextHookEx` 把消息传下去，或直接返回拦截。

**关键：**

- `dwThreadId = 0` 表示**全局钩子**——需要把 hook 函数所在的 DLL 注入到目标进程才能工作（OS 帮你做注入，但要求 hook 函数必须在 DLL 里，且**进程位数要匹配**：x64 进程只能被 x64 DLL hook）。
- `dwThreadId != 0` 表示**线程钩子**——只对指定线程生效，hook 函数可以在 EXE 里（不需要 DLL）。
- 某些钩子只支持线程级或只支持全局（见下表）。

### 2.2 14 种钩子类型

| `idHook` | 说明 | 全局/线程 | 注入要求 |
|---|---|---|---|
| `WH_KEYBOARD_LL` | 低级键盘钩子（按键、按下、释放） | 全局 | 不需 DLL（OS 用独立线程派发）|
| `WH_MOUSE_LL` | 低级鼠标钩子 | 全局 | 不需 DLL |
| `WH_KEYBOARD` | 普通键盘钩子（WM_KEYDOWN/UP 前） | 全局/线程 | 全局需 DLL |
| `WH_MOUSE` | 普通鼠标钩子 | 全局/线程 | 全局需 DLL |
| `WH_GETMESSAGE` | `GetMessage` 取到消息后 | 全局/线程 | 全局需 DLL |
| `WH_CALLWNDPROC` | 消息派发到窗口过程前 | 全局/线程 | 全局需 DLL |
| `WH_CALLWNDPROCRET` | 窗口过程返回后 | 全局/线程 | 全局需 DLL |
| `WH_MSGFILTER` | 模态循环（菜单、对话框）消息 | 仅线程 | – |
| `WH_SYSMSGFILTER` | 同上但全局 | 全局 | 需 DLL |
| `WH_CBT` | "Computer-Based Training" 钩子，窗口创建、激活、最小化等系统事件 | 全局/线程 | 全局需 DLL |
| `WH_SHELL` | Shell 事件（任务栏、托盘） | 全局/线程 | 全局需 DLL |
| `WH_JOURNALRECORD` | 录制消息（Win10+ 受限） | 全局 | 不需 DLL |
| `WH_JOURNALPLAYBACK` | 回放（Win10+ 受限） | 全局 | – |
| `WH_DEBUG` | 调试其他钩子 | 全局/线程 | – |
| `WH_FOREGROUNDIDLE` | 前台线程空闲 | 全局/线程 | 全局需 DLL |

> **重要变化**：自 Windows 10 起，`SetWindowsHookEx` 的全局钩子受 UIPI（User Interface Privilege Isolation）限制：**低完整性级别的钩子收不到高完整性进程的消息**。`WH_JOURNAL*` 已基本废弃，自动化建议用 UI Automation。

### 2.3 完整示例：低级键盘钩子（无需 DLL）

最简单且最实用的一个——`WH_KEYBOARD_LL`，OS 直接在你的消息循环里派发，不需要注入。

```c
// keylogger.c —— 教学用，绝非鼓励用作恶意软件
[[include]] <windows.h>
[[include]] <stdio.h>

HHOOK g_hook;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            printf("VK=0x%02lX SCAN=0x%02lX\n", p->vkCode, p->scanCode);
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

int main(void) {
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                               GetModuleHandleW(NULL), 0);
    if (!g_hook) { printf("hook failed: %lu\n", GetLastError()); return 1; }

    // 钩子需要一个消息循环来派发回调
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    UnhookWindowsHookEx(g_hook);
    return 0;
}
```

编译（用 MSVC）：

```cmd
cl /W4 /nologo keylogger.c user32.lib
```

**注意点**：

1. 必须有消息循环。`WH_KEYBOARD_LL` 的回调由 OS 在你的**主线程**上派发，没有循环就拿不到消息。
2. 回调里**不要做耗时操作**——Windows 有 `LowLevelHooksTimeout`（默认 300 ms）超时机制，超时会自动移除钩子。
3. 卸载用 `UnhookWindowsHookEx`，进程退出会自动卸载。
4. 不要在回调里 `printf` 到 console 后调用阻塞 IO——上面例子能用是因为 console 写入很快。生产环境用 ring buffer。

### 2.4 全局 CBT 钩子（需要 DLL）

要监控**所有进程**的窗口创建，必须用 DLL：

```c
// cbt_hook.c —— 编译为 cbt_hook.dll
[[include]] <windows.h>

[[pragma]] data_seg("SHARED")
HHOOK g_hook = NULL;
[[pragma]] data_seg()
[[pragma]] comment(linker, "/SECTION:SHARED,RWS")

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_CREATEWND) {
        // 新窗口被创建
        HWND hwnd = (HWND)wParam;
        OutputDebugStringA("CBT: HCBT_CREATEWND\n");
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

__declspec(dllexport) BOOL InstallHook(void) {
    HMODULE hMod = GetModuleHandleW(L"cbt_hook.dll");
    g_hook = SetWindowsHookExW(WH_CBT, CBTProc, hMod, 0);
    return g_hook != NULL;
}

__declspec(dllexport) BOOL UninstallHook(void) {
    return UnhookWindowsHookEx(g_hook);
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID l) { return TRUE; }
```

宿主程序（EXE）加载 DLL 并调用 `InstallHook`。OS 看到全局钩子，会把 `cbt_hook.dll` **自动注入**所有有 UI 的进程，每个进程独立调用 `CBTProc`。

**位数匹配**：x64 进程只接受 x64 hook DLL，32 位同理。要同时 hook 32/64 位需要装两份 DLL 和两份宿主。

---

## 3. IAT Hook —— 替换导入表函数指针

### 3.1 原理

PE 文件的 **Import Address Table (IAT)** 是一张函数指针表：当 EXE/DLL 调用 `MessageBoxW` 时，编译器生成的代码实际是：

```asm
call qword ptr [MessageBoxW_IAT]    ; 间接调用
```

`MessageBoxW_IAT` 是 IAT 里的一个 8 字节槽，启动时由 loader 填入 `user32!MessageBoxW` 的真实地址。

**IAT Hook 就是把这个槽改成你的函数地址**。原函数照常存在，但所有"通过本模块导入表调用"的地方都改走你的代码。

**局限**：
- 只能拦截**通过 IAT 调用**的。如果代码用 `GetProcAddress` 拿到地址再调，绕过 IAT，hook 失效。
- 必须**为每个模块单独 hook**——MyApp.exe 的 IAT、msvcrt.dll 的 IAT、user32.dll 的 IAT 是独立的表。
- 优势：完全用内存改写，无指令补丁，安全软件 / PatchGuard 完全不报警。

### 3.2 完整可运行示例

下面这个程序 hook 自己的 `MessageBoxW`：

```c
// iat_hook.c
[[include]] <windows.h>
[[include]] <stdio.h>

typedef int (WINAPI *MessageBoxW_t)(HWND, LPCWSTR, LPCWSTR, UINT);
static MessageBoxW_t g_realMessageBoxW = NULL;

int WINAPI HookedMessageBoxW(HWND h, LPCWSTR text, LPCWSTR caption, UINT type) {
    wprintf(L"[HOOK] MessageBoxW caption=\"%s\" text=\"%s\"\n",
            caption ? caption : L"", text ? text : L"");
    return g_realMessageBoxW(h, L"(hooked)", L"intercepted", type);
}

BOOL HookIAT(HMODULE hMod, LPCSTR dllName, LPCSTR funcName, PVOID newFunc, PVOID *oldFunc) {
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hMod;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dos->e_lfanew);
    DWORD impDirRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!impDirRVA) return FALSE;

    PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hMod + impDirRVA);
    for (; imp->Name; imp++) {
        LPCSTR mod = (LPCSTR)((BYTE*)hMod + imp->Name);
        if (_stricmp(mod, dllName) != 0) continue;

        PIMAGE_THUNK_DATA origThunk = (PIMAGE_THUNK_DATA)((BYTE*)hMod + imp->OriginalFirstThunk);
        PIMAGE_THUNK_DATA iat       = (PIMAGE_THUNK_DATA)((BYTE*)hMod + imp->FirstThunk);
        for (; origThunk->u1.AddressOfData; origThunk++, iat++) {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) continue;
            PIMAGE_IMPORT_BY_NAME byName = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hMod + origThunk->u1.AddressOfData);
            if (strcmp((char*)byName->Name, funcName) != 0) continue;

            DWORD oldProtect;
            VirtualProtect(&iat->u1.Function, sizeof(PVOID), PAGE_READWRITE, &oldProtect);
            if (oldFunc) *oldFunc = (PVOID)iat->u1.Function;
            iat->u1.Function = (ULONGLONG)newFunc;
            VirtualProtect(&iat->u1.Function, sizeof(PVOID), oldProtect, &oldProtect);
            return TRUE;
        }
    }
    return FALSE;
}

int wmain(void) {
    HookIAT(GetModuleHandleW(NULL), "user32.dll", "MessageBoxW",
            HookedMessageBoxW, (PVOID*)&g_realMessageBoxW);
    MessageBoxW(NULL, L"hello", L"caption", MB_OK);
    return 0;
}
```

编译运行：

```cmd
cl /W4 /nologo iat_hook.c user32.lib
iat_hook.exe
```

控制台输出：
```
[HOOK] MessageBoxW caption="caption" text="hello"
```
弹出的对话框文本变成 "(hooked)"。

### 3.3 EAT Hook（导出表）

EAT（Export Address Table）是 DLL 自己暴露给外部的函数表。改 EAT 主要影响**之后**才用 `GetProcAddress` 查询的调用方——已经 IAT 绑定好的调用方不受影响。所以 EAT Hook 适合"我控制目标 DLL，但不知道谁会来调"的场景，实战不如 IAT 常见。

---

## 4. Inline Hook —— 机器码改写

### 4.1 原理

Inline Hook 直接**改写目标函数的开头几字节**，把它们替换成 `jmp` 到你的 detour 函数。被覆盖的原指令保存在 trampoline 里，detour 函数想调原函数时通过 trampoline 跳回原函数第 N 字节继续执行。

```
原函数 OriginalFunc:                Trampoline:                   Detour:
+0:  push rbp                       +0:  push rbp                  你的代码
+1:  mov rbp, rsp                   +1:  mov rbp, rsp              ...
+4:  ...    ← 改写前                +4:  jmp OriginalFunc+5  ───┐  jmp/call Trampoline ←┐
                                                                │                        │
改写后:                                                          │                        │
+0:  jmp Detour ───────────────────────────────────────────────┘                        │
+5:  ...   ← 原指令从 +5 继续                                                            │
                                                                                         │
                                                              ←──────────────────────────┘
```

x64 平台的 `jmp rel32` 是 5 字节，但要跳到任意 64 位地址需要 `jmp [rip+0] / dq target`（14 字节）或 `mov rax, target; jmp rax`（12 字节）。所以 x64 Inline Hook 通常需要覆盖目标函数开头至少 12-14 字节。

**关键挑战：覆盖的字节不能拆开一条指令**。x86/x64 是变长指令集，必须用反汇编器先确认从 +0 起的"完整指令边界"位置 ≥ 你要覆盖的长度。这个解决方案就是 **length disassembler engine（LDE）**——MinHook/Detours 都内置。

### 4.2 热补丁兼容（Hot Patching）

微软所有系统 DLL 都用 `/hotpatch` 编译，函数开头加了 5 字节 NOP（`mov edi, edi; nop; nop; nop`）和 2 字节 `mov edi, edi`，专门为 hook 留位置：

```asm
function:
   90 90 90 90 90           ; 5 字节 NOP（可改成 jmp rel32）
   8b ff                    ; mov edi, edi（可改成 jmp short -7 跳到上面的 jmp）
   ; 真正的函数代码
```

Hot patch hook 只需要：
1. 把 `mov edi, edi` 改成 `jmp short $-7` → 跳到前面的 5 字节
2. 把 5 字节 NOP 改成 `jmp rel32 detour`

这样只改 7 字节、且只改 NOP 区域，不动真实指令，无需 trampoline。**但 x64 上很多函数没有这个 padding**，所以现代 x64 hook 库（MinHook）默认还是 Inline Hook + trampoline。

### 4.3 MinHook 完整示例

MinHook 是开源的轻量 Inline Hook 库（BSD-2，~50KB），是目前最常用的方案。下面写一个完整可编译的程序，hook `kernel32!CreateFileW` 来记录所有文件打开。

#### 4.3.1 项目结构

```
hooks_demo/
  ├── minhook/            (https://github.com/TsudaKageyu/minhook clone 进来)
  │   ├── include/MinHook.h
  │   └── src/...
  ├── hook_createfile.c
  └── CMakeLists.txt
```

#### 4.3.2 hook_createfile.c

```c
[[include]] <windows.h>
[[include]] <stdio.h>
[[include]] "MinHook.h"

typedef HANDLE (WINAPI *CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
                                       DWORD, DWORD, HANDLE);

static CreateFileW_t g_origCreateFileW = NULL;

HANDLE WINAPI HookedCreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSec, DWORD dwCreation, DWORD dwFlags, HANDLE hTemplate)
{
    wprintf(L"[HOOK] CreateFileW(\"%s\", 0x%lX)\n",
            lpFileName ? lpFileName : L"(null)", dwDesiredAccess);
    return g_origCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                             lpSec, dwCreation, dwFlags, hTemplate);
}

int wmain(void) {
    if (MH_Initialize() != MH_OK) { puts("MH_Initialize failed"); return 1; }

    if (MH_CreateHook(&CreateFileW, &HookedCreateFileW,
                      (LPVOID*)&g_origCreateFileW) != MH_OK) {
        puts("MH_CreateHook failed"); return 2;
    }
    if (MH_EnableHook(&CreateFileW) != MH_OK) { puts("MH_EnableHook failed"); return 3; }

    // 触发一次
    HANDLE h = CreateFileW(L"C:\\Windows\\System32\\notepad.exe",
                           GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);

    MH_DisableHook(&CreateFileW);
    MH_Uninitialize();
    return 0;
}
```

#### 4.3.3 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(hooks_demo C)
set(CMAKE_C_STANDARD 11)

add_library(minhook STATIC
  minhook/src/buffer.c
  minhook/src/hde/hde32.c
  minhook/src/hde/hde64.c
  minhook/src/hook.c
  minhook/src/trampoline.c)
target_include_directories(minhook PUBLIC minhook/include)

add_executable(hook_createfile hook_createfile.c)
target_link_libraries(hook_createfile PRIVATE minhook)
```

#### 4.3.4 构建运行

```cmd
cmake -B build -A x64
cmake --build build --config Release
build\Release\hook_createfile.exe
```

输出：
```
[HOOK] CreateFileW("C:\Windows\System32\notepad.exe", 0x80000000)
```

#### 4.3.5 内部发生了什么

`MH_CreateHook` 内部步骤：

1. **反汇编 `CreateFileW` 前若干字节**（通过 HDE，hacker disassembler engine）找到至少 14 字节的完整指令序列。
2. **在目标 ±2GB 范围内分配 trampoline 内存**（`VirtualAlloc` 选取地址，这样可用 `jmp rel32`）。
3. 把原始指令**复制到 trampoline**，并 fixup 任何相对寻址（RIP-relative `mov`、相对 `jmp/call`）。
4. trampoline 末尾追加 `jmp` 跳回 `CreateFileW + N` 继续。
5. **改写 `CreateFileW` 开头**为 `jmp` 到 detour——需要 `VirtualProtect` 改 page 为可写。
6. `g_origCreateFileW` 指向 trampoline。

调用关系变成：

```
程序调 CreateFileW → 跳到 HookedCreateFileW → 调 g_origCreateFileW（即 trampoline）→ 执行原前 N 字节 → 跳回 CreateFileW+N → 正常返回
```

### 4.4 Detours 简介

Microsoft Research 出品的 [Detours](https://github.com/microsoft/Detours) 是更老牌的方案，API 风格不同：

```c
DetourTransactionBegin();
DetourUpdateThread(GetCurrentThread());
DetourAttach(&(PVOID&)g_origCreateFileW, HookedCreateFileW);
DetourTransactionCommit();
```

特点：
- **事务式 API**：一次提交一组 hook。
- 内置 `DetourCreateProcessWithDll` 在子进程启动时自动注入。
- 商业用途要购买授权（3.x 开始变 MIT，新项目可用）。

### 4.5 x64 跳转编码细节

为什么 x64 Inline Hook 经常需要 14 字节？

- `jmp rel32`：5 字节，目标必须在 ±2GB 内。
- `jmp [rip+0]` 后跟 8 字节目标：`FF 25 00 00 00 00` + 8 字节地址 = 14 字节。
- `mov rax, imm64; jmp rax`：`48 B8 xx xx xx xx xx xx xx xx FF E0` = 12 字节，但破坏 rax。

实践中：
- 如果能在目标函数 ±2GB 内分配 trampoline → 用 `jmp rel32`（只需 5 字节覆盖）。
- 否则 → 14 字节绝对跳转。MinHook 默认尝试前者。

### 4.6 线程安全（quiesce）

如果在你 hook 时，另一个线程正好执行到 `CreateFileW` 开头的那几个字节中间——崩溃。Detours 通过 `DetourUpdateThread` 暂停指定线程并改写 IP；MinHook 在 `MH_EnableHook` 时遍历所有线程，检查它们的 RIP 是否落在改写区间内，是则等待或重定位。

**简化思路**：单线程程序无忧；多线程程序在程序启动早期 hook（其他工作线程还没起）是最安全的做法。

---

## 5. VEH Hook —— 借异常分发实现 hook

### 5.1 原理

Windows 提供 **Vectored Exception Handler**，比 SEH 更早被异常调用：

```c
PVOID AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler);
```

思路：
1. 在目标函数开头写 `int 3`（`CC`，单字节断点）或访问保护页造成异常。
2. CPU 触发异常 → 进入 VEH。
3. VEH 里检查 `ExceptionInfo->ContextRecord->Rip`，若是 hook 点，修改 `Rip` 跳到 detour。
4. 返回 `EXCEPTION_CONTINUE_EXECUTION`。

**优点**：
- 只改一个字节，对 PatchGuard 友好（PG 不扫所有页 `int 3`）。
- 容易做"条件 hook"——VEH 里判定。

**缺点**：
- 异常分发开销很大（每次调用 ~微秒级），hot path 不能用。
- 容易和调试器冲突——调试器也看 `int 3`。

实战中 VEH Hook 主要用于反作弊/反调试游戏作弊辅助检测，正常应用程序很少用。

---

## 6. DLL 注入的 4 种方式

要给目标进程"装 hook"，前提是你的代码要在目标进程地址空间里跑。注入方式：

### 6.1 CreateRemoteThread + LoadLibrary

最经典：

```c
HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPid);
LPVOID remoteMem = VirtualAllocEx(hProc, NULL, strlen(dllPath)+1, MEM_COMMIT, PAGE_READWRITE);
WriteProcessMemory(hProc, remoteMem, dllPath, strlen(dllPath)+1, NULL);

HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
LPTHREAD_START_ROUTINE pLoadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA");
HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, pLoadLib, remoteMem, 0, NULL);
WaitForSingleObject(hThread, INFINITE);
```

**优势**：简单可靠。
**缺点**：现代 EDR/AV 会监控 `CreateRemoteThread`。

### 6.2 SetWindowsHookEx

如 §2 所述，全局钩子会自动把 DLL 加载到所有有 UI 的进程。无需自己写注入代码。

### 6.3 AppInit_DLLs / Image File Execution Options

注册表 `HKLM\Software\Microsoft\Windows NT\CurrentVersion\Windows\AppInit_DLLs`，所有加载 user32.dll 的进程会自动加载列出的 DLL。

**Windows 10 起被 Secure Boot 默认禁用**，需要关闭 Secure Boot 才生效。生产环境不推荐。

### 6.4 NtMapViewOfSection / 反射式注入（Manual Map）

绕过 `LoadLibrary`，自己实现 PE loader 把 DLL 映射进目标。无文件落地（DLL 可直接从内存中的字节流加载），常用于安全研究和某些 EDR 规避。

```
读 DLL 字节 → VirtualAllocEx → WriteProcessMemory → 解析 PE → fixup relocation → fixup IAT → CreateRemoteThread 调用 DllMain
```

实现复杂、易和 EDR 对抗，正常用途几乎不用。

---

## 7. 进阶：PatchGuard / Kernel Patch Protection

x64 Windows 内核启用 **PatchGuard（KPP）**，会定期 checksum 关键内核数据结构（IDT、SSDT、GDT、MSR、内核代码段），发现被改写就蓝屏（BSOD 0x109 CRITICAL_STRUCTURE_CORRUPTION）。

**这意味着内核态 hook 在 x64 上几乎不可行**——除非你愿意：
- 绕过 PG（每个 Windows 大版本更新都要重写绕过代码，极其脆弱）；
- 用微软提供的合规接口（Filter Manager / WFP / Object Manager Callbacks / Process Notify）。

正经做内核监控应该用：

| 目标 | 推荐 API |
|---|---|
| 拦截文件 IO | Filter Manager（minifilter driver）|
| 拦截网络 | Windows Filtering Platform (WFP) |
| 进程/线程创建通知 | `PsSetCreateProcessNotifyRoutineEx` |
| 注册表通知 | `CmRegisterCallbackEx` |
| 内核对象访问通知 | `ObRegisterCallbacks` |

这些都是微软批准的"hook"机制，PG 不会管。

---

## 8. 调试与卸载

### 8.1 调试

- **用调试符号**：`SymInitialize` + `SymFromAddr`，能在 hook 里打印调用栈。
- **WinDbg 看 trampoline**：`!chkimg` 检查模块是否被改写、`u <addr>` 反汇编 trampoline。
- **MinHook 的 `MH_STATUS` 返回值要全部检查**，不要忽略——失败原因（`MH_ERROR_ALREADY_CREATED`/`MH_ERROR_NOT_EXECUTABLE` 等）一目了然。
- **Procmon** 可以验证 IAT hook 是否生效（看 stack trace 里是否经过你的模块）。

### 8.2 卸载

| 方式 | 卸载 |
|---|---|
| `SetWindowsHookEx` | `UnhookWindowsHookEx`（DLL 也会自动从所有进程卸载）|
| IAT Hook | 把原指针写回 IAT 槽 |
| Inline Hook (MinHook/Detours) | `MH_DisableHook` / `DetourDetach` |
| VEH Hook | `RemoveVectoredExceptionHandler` |
| Manual Map 注入 | 自己写 unloader：`FreeLibrary`-style 反向解析 |

**卸载 Inline Hook 时的关键**：必须保证此刻没有线程的 RIP 在 trampoline 内或目标函数被改写的字节范围内。MinHook 在 `MH_DisableHook` 时会做检查。

---

## 9. 常见陷阱

| 陷阱 | 表现 | 原因 | 修法 |
|---|---|---|---|
| Hook DLL 位数不匹配 | `SetWindowsHookEx` 返回 0，GetLastError=1428 | x64 进程拒绝 32 位 hook DLL | 编译对应位数 |
| `WH_KEYBOARD_LL` 没消息循环 | 钩子注册成功但收不到回调 | 低级钩子由 OS 在调用线程派发，需要 GetMessage 循环 | 加消息循环 |
| 钩子回调阻塞 | 整个系统输入变卡 / 钩子被自动卸载 | 超过 `LowLevelHooksTimeout` | 回调里只入队，让另一线程处理 |
| IAT Hook 对动态加载无效 | hook 后某些调用仍走原函数 | 调用方用 `GetProcAddress`/`LoadLibrary` 后调用，绕过 IAT | 用 Inline Hook 或同时 hook `GetProcAddress` |
| Inline Hook 崩溃 | 进程在 hook 后立即 AV | 覆盖了不完整的指令、或目标 ±2GB 范围内分不到 trampoline | 用成熟库（MinHook），不要手撸 |
| 多线程下装 hook 崩溃 | 偶发崩溃在 trampoline 中间 | 改写时有线程正执行被改字节 | 用 `DetourUpdateThread` 暂停所有线程 |
| Detour 里递归调用自己 | 无限递归 → 栈溢出 | 在 detour 里调被 hook 的函数走的还是 detour | 用 trampoline 调原函数，或加 thread-local 守卫 |
| 注入到受保护进程 | OpenProcess 失败 ERROR_ACCESS_DENIED 5 | PPL（Protected Process Light，如 LSASS、AV 进程） | 不能，除非你也是 PPL |
| 数字签名进程拒绝注入 | LoadLibrary 失败 | Process Mitigation Policy: `PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY` | 给注入 DLL 签名，或用 Manual Map |

---

## 10. 参考资料

- Microsoft Detours: https://github.com/microsoft/Detours
- MinHook: https://github.com/TsudaKageyu/minhook
- *Programming Applications for Microsoft Windows* (Jeffrey Richter) —— 第 22 章 DLL Injection and API Hooking 仍是经典
- *Windows Internals, 7th Edition* —— 系统钩子和 KPP 章节
- PE 格式：https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- SetWindowsHookEx 文档：https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw
- Filter Manager（内核合规 hook）：https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/filter-manager-concepts

下一篇：[03-linux-hook.md](./03-linux-hook.md)
