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

### 3.3 EAT Hook（导出地址表改写）

#### 3.3.1 原理：IAT 与 EAT 的对称性

IAT Hook 改的是**调用方**侧的函数指针（"你调 X，帮我改成调 Y"）；EAT Hook 改的是**被调方**侧的导出地址（"你暴露 X，帮我改成暴露 Y"）。二者是一枚硬币的两面：

```
IAT Hook：
  调用方模块.IAT[MessageBoxW] = HookedMessageBoxW  ← 只影响此模块的调用

EAT Hook：
  user32.dll.EAT[MessageBoxW的RVA] = HookedMessageBoxW的RVA  ← 影响所有之后用 GetProcAddress 查询的调用方
```

PE 文件中导出目录结构（`IMAGE_EXPORT_DIRECTORY`）：

```
IMAGE_EXPORT_DIRECTORY
  .NumberOfFunctions    = 总导出槽数
  .NumberOfNames        = 按名导出数（≤ NumberOfFunctions）
  .AddressOfFunctions   → RVA数组，下标是「序号 - Base」，存的是函数入口 RVA
  .AddressOfNames       → 函数名字符串 RVA 数组
  .AddressOfNameOrdinals→ 名字→序号的映射数组（AddressOfNames[i] 对应序号 AddressOfNameOrdinals[i]）
```

`GetProcAddress("MessageBoxW")` 的内部路径：
1. 二分查找 `AddressOfNames` 找到 "MessageBoxW" 的下标 `i`
2. 读 `ordinal = AddressOfNameOrdinals[i]`
3. 读 `rva = AddressOfFunctions[ordinal]`
4. 返回 `hMod + rva`

**EAT Hook 就是修改第 3 步的 `rva`，让 `GetProcAddress` 返回你的 detour 地址。**

#### 3.3.2 有效范围与局限

EAT Hook **只影响 hook 写入之后**才调用 `GetProcAddress` 查询该函数的模块。原因：
- 大多数 DLL 在 `LoadLibrary` 时就已经完成 IAT 解析，把函数地址缓存进了各模块的 IAT 槽里。改 EAT 后，这些已缓存的 IAT 指针不会更新。
- 只有在 hook 写入后才新加载的模块（或运行时动态调用 `GetProcAddress` 的代码）会受到影响。

典型适用场景：你想拦截某 DLL 被其他（尚未加载的）插件调用的导出函数，且你能在那些插件加载前写入 EAT Hook。

#### 3.3.3 完整代码骨架

下面演示对自身进程内某 DLL 做 EAT Hook——将 `kernel32.dll` 导出的 `OutputDebugStringA` 改写，使所有之后 `GetProcAddress` 查询到的地址指向我们的 detour：

```c
// eat_hook.c
#include <windows.h>
#include <stdio.h>

// ------- Detour 函数 -------
typedef void (WINAPI *OutputDebugStringA_t)(LPCSTR);
static OutputDebugStringA_t g_origOutputDebugStringA = NULL;

void WINAPI HookedOutputDebugStringA(LPCSTR lpOutputString) {
    printf("[EAT HOOK] OutputDebugStringA: %s\n",
           lpOutputString ? lpOutputString : "(null)");
    if (g_origOutputDebugStringA)
        g_origOutputDebugStringA(lpOutputString);
}

// ------- EAT Hook 核心 -------
// 返回 TRUE 表示成功，oldRVA 输出原 RVA（可用于卸载）
BOOL HookEAT(HMODULE hMod, LPCSTR funcName, PVOID detour, DWORD *oldRVA) {
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hMod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    DWORD expDirRVA = nt->OptionalHeader
                         .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
                         .VirtualAddress;
    if (!expDirRVA) return FALSE;

    PIMAGE_EXPORT_DIRECTORY expDir =
        (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hMod + expDirRVA);

    DWORD  *rvaFuncs    = (DWORD*)((BYTE*)hMod + expDir->AddressOfFunctions);
    DWORD  *rvaNames    = (DWORD*)((BYTE*)hMod + expDir->AddressOfNames);
    WORD   *nameOrdinals= (WORD*) ((BYTE*)hMod + expDir->AddressOfNameOrdinals);

    // 按名查序号（线性扫描；函数数量有限，无需二分）
    for (DWORD i = 0; i < expDir->NumberOfNames; i++) {
        LPCSTR name = (LPCSTR)((BYTE*)hMod + rvaNames[i]);
        if (strcmp(name, funcName) != 0) continue;

        WORD ordinal = nameOrdinals[i];  // 相对于 Base 的 ordinal（已是 0-based 下标）

        // 保存原 RVA
        if (oldRVA) *oldRVA = rvaFuncs[ordinal];

        // 计算 detour 的 RVA（必须是同一 hMod 基址内的地址，否则 RVA 意义不同）
        // 注意：detour 可以是任意可执行地址，不必在 hMod 内；
        // 但 GetProcAddress 若检测到 RVA 落在 Forwarded Export 范围则特殊处理——
        // 为避免此问题，detour 地址应当位于 hMod 的映射范围外（或确认 RVA 不落入导出目录范围）。
        DWORD newRVA = (DWORD)((BYTE*)detour - (BYTE*)hMod);

        DWORD oldProt;
        VirtualProtect(&rvaFuncs[ordinal], sizeof(DWORD), PAGE_READWRITE, &oldProt);
        rvaFuncs[ordinal] = newRVA;
        VirtualProtect(&rvaFuncs[ordinal], sizeof(DWORD), oldProt, &oldProt);
        return TRUE;
    }
    return FALSE;  // 未找到该函数名
}

int main(void) {
    HMODULE hK32 = GetModuleHandleW(L"kernel32.dll");

    // 先保存原始函数指针（供 detour 调用原函数用）
    g_origOutputDebugStringA =
        (OutputDebugStringA_t)GetProcAddress(hK32, "OutputDebugStringA");

    DWORD oldRVA = 0;
    if (!HookEAT(hK32, "OutputDebugStringA", HookedOutputDebugStringA, &oldRVA)) {
        puts("HookEAT failed");
        return 1;
    }
    puts("EAT hook installed.");

    // 验证：此后 GetProcAddress 返回的是 detour 地址
    OutputDebugStringA_t fn =
        (OutputDebugStringA_t)GetProcAddress(hK32, "OutputDebugStringA");
    printf("GetProcAddress now returns: %p (detour: %p, match: %d)\n",
           (void*)fn, (void*)HookedOutputDebugStringA,
           fn == HookedOutputDebugStringA);

    // 通过新地址调用——触发 hook
    fn("hello from EAT hook\n");

    // 卸载：恢复原 RVA
    DWORD *rvaFuncs = NULL;
    {
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hK32;
        PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((BYTE*)hK32 + dos->e_lfanew);
        DWORD expDirRVA = nt->OptionalHeader
                             .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
                             .VirtualAddress;
        PIMAGE_EXPORT_DIRECTORY expDir =
            (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hK32 + expDirRVA);
        rvaFuncs = (DWORD*)((BYTE*)hK32 + expDir->AddressOfFunctions);
        WORD *nameOrdinals = (WORD*)((BYTE*)hK32 + expDir->AddressOfNameOrdinals);
        DWORD *rvaNames    = (DWORD*)((BYTE*)hK32 + expDir->AddressOfNames);
        for (DWORD i = 0; i < expDir->NumberOfNames; i++) {
            if (strcmp((LPCSTR)((BYTE*)hK32 + rvaNames[i]), "OutputDebugStringA") == 0) {
                WORD ord = nameOrdinals[i];
                DWORD oldProt;
                VirtualProtect(&rvaFuncs[ord], sizeof(DWORD), PAGE_READWRITE, &oldProt);
                rvaFuncs[ord] = oldRVA;
                VirtualProtect(&rvaFuncs[ord], sizeof(DWORD), oldProt, &oldProt);
                break;
            }
        }
    }
    puts("EAT hook removed.");
    return 0;
}
```

#### 3.3.4 一个重要陷阱：Forwarded Export

PE 格式里，若 `AddressOfFunctions[i]` 的 RVA **落在导出目录本身的范围内**（`expDirRVA` 到 `expDirRVA + expDirSize`），`GetProcAddress` 把它当作**转发字符串**（如 `"NTDLL.RtlMoveMemory"`）而不是函数地址——这是一个完全不同的代码路径。如果你的 detour 地址计算出的 RVA 恰好落在这个窗口里，hook 会静默失效且行为不可预测。规避方法：确保 detour 位于目标模块的映射范围之外（即 RVA 超出模块大小），或者在写入前用 assert 检查 `newRVA < expDirRVA || newRVA >= expDirRVA + expDirSize`。

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

#### 6.4.1 为什么要绕过 LoadLibrary

`LoadLibrary`（内部走 `LdrLoadDll`）加载 DLL 时，Windows 做了以下**可被安全产品监控的动作**：

1. 通知 `PsSetLoadImageNotifyRoutine` 注册的内核回调（EDR 驱动常用此处）
2. 将模块插入进程的 `PEB.Ldr` 链表（`InLoadOrderModuleList` 等三条链）
3. 调用 `DllMain(DLL_PROCESS_ATTACH)`——很多 AV 在此时扫描内存
4. 留下可查询的模块记录，`CreateToolhelp32Snapshot`/`EnumProcessModules` 可枚举

Manual Map（手工映射）完全自己实现这套逻辑，**只做业务逻辑必须做的步骤**，跳过一切会产生系统记录的路径。最终目标 DLL 在目标进程内存里执行，但对工具层完全不可见——是内存取证级别才能发现的存在。

#### 6.4.2 完整步骤分解

```
1. 读 DLL 字节流到注入进程内存（或直接内存中构造）
2. OpenProcess → VirtualAllocEx 在目标进程分配足够空间（SizeOfImage）
3. WriteProcessMemory 写入 PE 头和各节区（按节区对齐量）
4. 修复重定位（若加载地址 ≠ DLL 的 ImageBase，遍历 .reloc 节做 delta 修正）
5. 修复 IAT（解析 IMAGE_IMPORT_DESCRIPTOR，对每条 thunk 调用 GetProcAddress）
6. 调用 DllMain：CreateRemoteThread 传入 DllMain 地址 + DLL_PROCESS_ATTACH
```

每一步都可能出错，且必须在目标进程的地址空间语境下理解——分配到的远程地址就是"加载基址"，所有 RVA 从它算起。

#### 6.4.3 可运行骨架实现

下面是一个自注入（注入自身进程）的最小 Manual Map 示例，方便在本地调试理解原理；实际远程注入只需把 `VirtualAlloc` 换成 `VirtualAllocEx`，内存写入换成 `WriteProcessMemory`，DllMain 调用换成 `CreateRemoteThread`。

```c
// manual_map_local.c
// 自注入：把一个 DLL 字节流（不经 LoadLibrary）映射进当前进程并执行 DllMain。
// 编译：cl /W4 /nologo manual_map_local.c
#include <windows.h>
#include <stdio.h>

// 使用场景：szDllPath 是普通磁盘路径；实际对抗场景中字节流可来自网络/资源节。
HMODULE ManualMap(LPCSTR szDllPath) {
    // 1. 读 DLL 文件到堆缓冲区
    HANDLE hFile = CreateFileA(szDllPath, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) { puts("open failed"); return NULL; }
    DWORD fileSize = GetFileSize(hFile, NULL);
    BYTE *rawBuf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, fileSize);
    DWORD bytesRead;
    ReadFile(hFile, rawBuf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)rawBuf;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)(rawBuf + dos->e_lfanew);

    // 2. 分配映射内存（IMAGE_OPTIONAL_HEADER.SizeOfImage 是节区全展开后的大小）
    BYTE *base = (BYTE*)VirtualAlloc(NULL, nt->OptionalHeader.SizeOfImage,
                                     MEM_COMMIT | MEM_RESERVE,
                                     PAGE_EXECUTE_READWRITE);
    if (!base) { puts("VirtualAlloc failed"); HeapFree(GetProcessHeap(),0,rawBuf); return NULL; }

    // 3. 复制 PE 头（SizeOfHeaders 字节）
    memcpy(base, rawBuf, nt->OptionalHeader.SizeOfHeaders);

    // 4. 按节区复制各 section
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (sec->SizeOfRawData == 0) continue;
        memcpy(base + sec->VirtualAddress,
               rawBuf + sec->PointerToRawData,
               sec->SizeOfRawData);
    }
    HeapFree(GetProcessHeap(), 0, rawBuf);

    // 5. 修复重定位
    // delta = 实际加载基址 - 期望基址（ImageBase）
    LONGLONG delta = (LONGLONG)(base - (BYTE*)(ULONG_PTR)nt->OptionalHeader.ImageBase);
    if (delta != 0) {
        DWORD relocRVA  = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
        DWORD relocSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
        if (relocRVA && relocSize) {
            PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)(base + relocRVA);
            while (reloc->VirtualAddress) {
                WORD *entries = (WORD*)((BYTE*)reloc + sizeof(IMAGE_BASE_RELOCATION));
                DWORD count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                for (DWORD i = 0; i < count; i++) {
                    WORD type   = entries[i] >> 12;
                    WORD offset = entries[i] & 0x0FFF;
                    if (type == IMAGE_REL_BASED_DIR64) {
                        // x64：64 位绝对地址需要加 delta
                        ULONGLONG *addr = (ULONGLONG*)(base + reloc->VirtualAddress + offset);
                        *addr += (ULONGLONG)delta;
                    } else if (type == IMAGE_REL_BASED_HIGHLOW) {
                        // x86：32 位绝对地址
                        DWORD *addr = (DWORD*)(base + reloc->VirtualAddress + offset);
                        *addr += (DWORD)delta;
                    }
                    // IMAGE_REL_BASED_ABSOLUTE (type=0) 是填充项，跳过
                }
                reloc = (PIMAGE_BASE_RELOCATION)((BYTE*)reloc + reloc->SizeOfBlock);
            }
        }
    }

    // 6. 修复 IAT（Import Address Table）
    DWORD impRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (impRVA) {
        PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR)(base + impRVA);
        for (; imp->Name; imp++) {
            LPCSTR modName = (LPCSTR)(base + imp->Name);
            HMODULE hDep = LoadLibraryA(modName);  // 依赖库走正常 LoadLibrary
            if (!hDep) { printf("LoadLibrary %s failed\n", modName); continue; }

            PIMAGE_THUNK_DATA origThunk = imp->OriginalFirstThunk
                ? (PIMAGE_THUNK_DATA)(base + imp->OriginalFirstThunk)
                : (PIMAGE_THUNK_DATA)(base + imp->FirstThunk);
            PIMAGE_THUNK_DATA iat = (PIMAGE_THUNK_DATA)(base + imp->FirstThunk);

            for (; origThunk->u1.AddressOfData; origThunk++, iat++) {
                if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                    // 按序号导入
                    iat->u1.Function = (ULONGLONG)GetProcAddress(
                        hDep, MAKEINTRESOURCEA(IMAGE_ORDINAL(origThunk->u1.Ordinal)));
                } else {
                    // 按名称导入
                    PIMAGE_IMPORT_BY_NAME byName =
                        (PIMAGE_IMPORT_BY_NAME)(base + origThunk->u1.AddressOfData);
                    iat->u1.Function = (ULONGLONG)GetProcAddress(hDep, (LPCSTR)byName->Name);
                }
            }
        }
    }

    // 7. 调用 DllMain(DLL_PROCESS_ATTACH)
    if (nt->OptionalHeader.AddressOfEntryPoint) {
        typedef BOOL (WINAPI *DllMain_t)(HINSTANCE, DWORD, LPVOID);
        DllMain_t dllMain = (DllMain_t)(base + nt->OptionalHeader.AddressOfEntryPoint);
        dllMain((HINSTANCE)base, DLL_PROCESS_ATTACH, NULL);
    }

    return (HMODULE)base;
}

int main(void) {
    // 示例：手工映射一个简单 DLL（需自行准备一个不依赖复杂 CRT 的测试 DLL）
    HMODULE h = ManualMap("test_payload.dll");
    if (!h) { puts("ManualMap failed"); return 1; }
    printf("Mapped at %p\n", (void*)h);
    // 调用导出函数（跳过 GetProcAddress——因为模块不在 PEB.Ldr 里）
    // 需手动按 EAT 偏移找函数地址，这里略去
    return 0;
}
```

#### 6.4.4 反射式 DLL 注入变体

"反射式"（Reflective）注入由 Stephen Fewer 在 2008 年提出：DLL 内部嵌入一段 `ReflectiveDllLoader` shellcode，注入者只需：

```
1. VirtualAllocEx + WriteProcessMemory 把 DLL 字节流写入目标
2. CreateRemoteThread 传入 DLL 内部 ReflectiveDllLoader 函数的偏移
3. ReflectiveDllLoader 在目标进程内自举（自己找 kernel32 的 GetProcAddress/LoadLibrary，
   完成上面的第 4-7 步），无需注入者做 PE 解析
```

这样注入者代码极简，全部复杂逻辑在 payload DLL 内部完成，且在目标进程里不留任何 `LoadLibrary` 调用记录。

#### 6.4.5 与内核监控的关系

绕过了 `LdrLoadDll` 就绕过了 `PsSetLoadImageNotifyRoutine`，这是为什么 EDR 开始在内核里用 **ETW TI（Thread and Process telemetry）** 和 **内存扫描回调**（`MmRegisterSecureMemoryNotification` 等）来发现 Manual Map：

| 监控手段 | 能否发现 Manual Map |
|---|---|
| `CreateToolhelp32Snapshot` 枚举模块 | 无法发现（不在 PEB.Ldr） |
| `PsSetLoadImageNotifyRoutine`（内核） | 无法发现（绕过 Ldr） |
| 内存页扫描（扫 MZ/PE 头） | **可以发现**（base 处仍有 MZ 头） |
| ETW Process/Image 事件 | **部分可发现**（取决于 Windows 版本） |
| 用户态钩子 `LdrLoadDll` | 无法发现（完全绕过） |

防御建议：不依赖 API 调用路径做检测，而是直接扫进程内存中的 PE 头特征（`MZ + PE`）并与 PEB 模块列表交叉对比，未出现在列表中的映像即可疑。

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
