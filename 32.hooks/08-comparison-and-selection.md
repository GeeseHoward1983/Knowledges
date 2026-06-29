# 08 · 综合对比与选型决策

> 本篇把前 7 篇里出现过的所有 hook 技术放在同一张桌子上**横向对比**，并给出**按目标反查技术**的决策手册。
> 适合作为"我要做 X，到底用什么"的查表入口。

---

## 1. 总览：13 类 hook 技术的统一对照

把前面所有篇章的核心技术汇总：

| # | 技术 | 抽象层 | 侵入性 | 粒度 | 时机 | 跨进程 | 持久性 | 性能开销 |
|---|---|---|---|---|---|---|---|---|
| 1 | React/Vue Hooks | 框架（应用代码内） | 显式注册 | 组件实例 | mount/update/unmount | 否 | 组件生命周期 | 极低 |
| 2 | Qt 事件过滤器 | 框架 | 显式注册 | QObject / app | event 派发各阶段 | 否 | 进程内 | 低（对象级）/ 中（全局） |
| 3 | Qt 原生事件过滤器 | OS 平台层 | 显式注册 | thread / app | OS 消息 | 否 | 进程内 | 低 |
| 4 | Win32 `SetWindowsHookEx` | OS 提供 | 显式注册 | 线程 / 全局 | 消息派发 | **是**（自动注入）| 注册期 | 低-中 |
| 5 | IAT/EAT Hook | PE 表 | 改内存 | 进程内 / 单模块 | 调用前 | 否 | 进程内 | 几乎零 |
| 6 | Inline Hook (MinHook/Detours) | 机器码 | 改指令 | 进程内 / 任意函数 | 调用前/后/替换 | 否（需注入）| 直到 unhook | 几乎零 |
| 7 | VEH Hook | OS 异常分发 | 改指令（断点） | 进程内 | 异常时 | 否 | 直到卸载 | 高（异常开销）|
| 8 | LD_PRELOAD | 动态链接 | 重定向符号 | 进程 | 调用前 | 否（仅自启）| 进程生命 | 几乎零 |
| 9 | ptrace / GDB 注入 | OS 调试接口 | attach + 改 reg/内存 | 进程 / 线程 | 任意指令 | 否（一次一进程）| 解 detach 即恢复 | 高（每 syscall）|
| 10 | eBPF (kprobe/uprobe/tracepoint/XDP/LSM) | 内核 | 注册到 hook 点 | 系统全局 | 函数入/出、syscall、网络包、LSM | **是**（全系统）| 直到卸载 | 极低 |
| 11 | Python (monkey/mock/settrace/audit/import) | 语言运行时 | 改对象引用 / API | 模块 / 全局 | 调用、行执行、import、审计 | 否 | 进程内 | 低-高（settrace 慢）|
| 12 | Node.js (Module._load, ESM Loader, async_hooks) | 语言运行时 | 改对象引用 / register | 模块 / 全局 | require/import、async lifecycle | 否 | 进程内 | 低-中 |
| 13 | Claude Code hooks | 工具 harness | 配置文件 | 工具 / 会话 | 工具前后、会话/消息事件 | 否 | settings 永久 | 低（外部进程）|
| 14 | Git hooks | VCS | 文件 + 路径配置 | 仓库 / push 事件 | commit/push/merge 等 | 是（推/拉时双方）| 永久（local 或 server） | 低 |

---

## 2. "我要做 X"——按目标反查技术

### 2.1 应用内 UI / 输入

| 目标 | 首选 | 备选 |
|---|---|---|
| 在自己写的 React 组件加状态/effect | useState/useEffect | Vue 等价 |
| 跨多个组件复用有状态逻辑 | 自定义 hook（`useXxx`） | render props（过时）|
| 拦截某个 widget 的按键 | Qt `installEventFilter` | 子类化 `keyPressEvent` |
| 全局快捷键 | Win32: `RegisterHotKey` + Qt native filter / GLFW / X11 grab | Qt `installEventFilter` 装到 `qApp` |
| 拦截窗口关闭 | Qt: 重写 `closeEvent` | event filter |
| 阻止 Dialog 上的 ESC | Qt event filter 吃 KeyPress | 重写 `keyPressEvent` |
| Web 全局监听 keydown | `document.addEventListener('keydown')` | 不要 monkey patch |

### 2.2 拦截 / 替换函数

| 目标 | 首选 | 备选 |
|---|---|---|
| 替换自己代码的某函数 | 直接改源码 | 装饰器 / Strategy 模式 |
| 替换第三方库的某函数（运行时） | Python: monkey patch / mock<br>JS: monkey assign | C: LD_PRELOAD / Inline Hook |
| 替换 libc malloc | `LD_PRELOAD` + `dlsym(RTLD_NEXT,...)` | jemalloc/tcmalloc 链接 |
| 监控 Win32 API（自己进程） | IAT Hook | Inline Hook |
| 监控 Win32 API（全系统）| ETW 提供方<br>EDR-level driver | DLL 注入 + Inline Hook（不推荐）|
| 监控 Linux syscall（全系统）| eBPF tracepoint `sys_enter_*` | strace（慢）|
| 监控某个已运行进程的函数 | uprobe + eBPF | GDB python script |
| 给现有 Python 类方法加日志 | 装饰器 / monkeypatch 类属性 | metaclass |
| 拦截所有 Node `require` | `Module._load` 改写 | ESM loader hook（ESM 部分）|
| 拦截所有 Node `import`（ESM） | `module.register(loader)` | — |

### 2.3 调用前后插桩 / Tracing / Profiling

| 目标 | 首选 | 备选 |
|---|---|---|
| Python 函数级 profile | `cProfile` / `sys.setprofile` | py-spy（采样）|
| Python 行级覆盖率 | `coverage.py`（基于 settrace） | `sys.monitoring`（3.12+）|
| Node.js CPU profile | Inspector Protocol `Profiler.start` | 0x / clinic.js |
| Linux 系统级采样 profiler | `perf record` / eBPF profile | bpftrace |
| Linux 系统级 syscall trace | eBPF tracepoint | strace（重）|
| Windows 系统级 trace | ETW | xperf / WPA |
| Qt 内 signal 触发监听 | `connect` 到额外 lambda | `QSignalSpy`（测试）/ `QInternal::registerCallback` |
| HTTP 出站请求审计（Node） | monkey patch `http.request`/`https.request` | OpenTelemetry instrumentation |
| HTTP 请求审计（Python） | `requests` Session hook / mock | mitmproxy（外置）|

### 2.4 全系统级 / 跨进程

| 目标 | 首选 | 备选 |
|---|---|---|
| 监控所有进程的文件 IO（Linux） | eBPF tracepoint syscalls | inotify（只看路径，不看进程）|
| 监控所有进程的文件 IO（Windows） | minifilter driver | Procmon |
| 全系统键盘 logger | Linux: eBPF uprobe `readline` / X11 grab；Win: WH_KEYBOARD_LL | – |
| 网络 DDoS 防护 | eBPF XDP | iptables（更慢）|
| 限制某进程能调哪些 syscall | seccomp-bpf | eBPF LSM |
| Linux 强制访问控制 | SELinux / AppArmor | eBPF LSM（5.7+）|
| Windows 文件操作审计 | minifilter | ETW + 用户态消费 |

### 2.5 调试 / 测试

| 目标 | 首选 | 备选 |
|---|---|---|
| Mock 一个第三方 API | Python: `unittest.mock.patch`<br>JS: `jest.mock` / `vi.mock` | 注入接口（设计层）|
| 断点调试 | gdb / lldb / pdb / Node Inspector | printf |
| Qt 信号触发断言（测试） | `QSignalSpy` | manual connect + flag |
| 拦截 Web 端 fetch 测试 | MSW (Mock Service Worker) | monkey patch `globalThis.fetch` |

### 2.6 工程流程 / 自动化

| 目标 | 首选 | 备选 |
|---|---|---|
| commit 前自动 format | Git pre-commit hook（pre-commit framework / husky / lefthook） | IDE 配置 format on save |
| commit message 强制规范 | Git commit-msg hook | 服务端 CI check |
| push 时跑测试 | Git pre-push hook + CI 兜底 | 仅 CI |
| 服务端禁止 push 到 main | GitHub branch protection / Git server-side hook | — |
| Claude Code 编辑后自动 lint | Claude PostToolUse(Edit) hook | git pre-commit（晚一步） |
| Claude Code 危险命令拦截 | PreToolUse(Bash) hook + 正则 | permissions.deny |
| Claude Code 完成时通知 | Stop hook | OS 通知 |
| 让 Claude 看到 git status | SessionStart hook | 用户每次手贴 |

### 2.7 安全 / 反作弊

| 目标 | 首选 | 备选 |
|---|---|---|
| 反作弊（游戏外挂检测） | Win32: PatchGuard 合规 + 用户态 anti-tamper；Linux: 内核模块 | VEH Hook 自检 |
| 防 DLL 注入 | Process Mitigation Policy: BinarySignaturePolicy | manual scan loaded modules |
| 防 LD_PRELOAD | setuid 程序自动免疫 / 启动时检查 `/proc/self/environ` | — |
| 检测密钥提交 | Git pre-commit (trufflehog/gitleaks) + 服务端 pre-receive | CI 扫 |
| 限制子进程 syscall | seccomp-bpf | bwrap / firejail |

---

## 3. 决策树：从"我有什么权限"反推

实际工作中决定能用什么 hook，第一道门是**权限和环境**。

```
你能改目标的源代码吗？
├─ 能 → 优先源码（hook 是 last resort）
└─ 不能 ↓

你能控制目标进程的启动吗？
├─ 能（自己 spawn） → LD_PRELOAD (Linux) / Detours injection (Win)
└─ 不能 ↓

你有 root / Administrator？
├─ 有 → ptrace / eBPF (Linux) / DLL 注入 (Win)
└─ 没有 ↓

只能在自己用户权限内 → 限制非常大；
  - Linux: 同 uid 进程 ptrace（看 yama）
  - Win: 同 session 同 integrity level
  - 或者：劝甲方装 hook → 退化为"功能开关"问题
```

---

## 4. 性能维度横评

按"每秒能承受多少次 hook 触发"粗估：

| 技术 | 量级（次/秒/核） | 备注 |
|---|---|---|
| Inline Hook（trampoline） | ~10^8 | 几乎零开销，等于多两条跳转 |
| IAT Hook | ~10^8 | 仅多一次间接跳转 |
| LD_PRELOAD 替换 | ~10^8 | 同上 |
| eBPF kprobe/tracepoint | ~10^7 | JIT 后接近原生 |
| eBPF XDP | ~10^7（千万 pps）| 在网卡驱动级 |
| Win32 `SetWindowsHookEx` 低级 | ~10^6 | 跨进程 IPC 路径 |
| Qt object event filter | ~10^7 | 一次虚函数调 |
| Qt 全局 event filter | 显著影响 UI | 每个事件遍历 list |
| React/Vue hook | n/a（用户操作级） | 不在 hot path |
| ptrace 中断每 syscall | ~10^4 | 两次上下文切换 |
| Python settrace 行级 | ~10^4 | 10x–100x 慢 |
| Python audit hook | ~10^6 | 每事件回调一次 |
| VEH Hook（异常）| ~10^5 | 异常分发昂贵 |
| Git hook | n/a（每次 git 操作 1 次） | 整体延迟 |
| Claude Code hook | n/a（每次 tool 调用 1 次） | 整体延迟 |

> 这些是数量级估计，具体看 handler 自身复杂度。规则：**hook 本身开销小，handler 里别写慢代码**。

---

## 5. 卸载安全度横评

可卸载性是工程化的关键。Inline Hook 装容易卸难——必须保证没有线程正在 trampoline 中执行。

| 技术 | 卸载安全度 | 卸载操作 |
|---|---|---|
| React/Vue hook | 完美 | 组件 unmount 自动 |
| Qt event filter | 完美 | `removeEventFilter`，但要保证 filter 对象还在 |
| `SetWindowsHookEx` | 完美 | `UnhookWindowsHookEx` |
| IAT Hook | 完美 | 把原指针写回 |
| LD_PRELOAD | 完美（进程退出）| 不能运行时取消 |
| Python monkey patch | 完美（保存原引用） | 赋回 |
| Node monkey patch | 完美 | 同上 |
| Python `sys.addaudithook` | **不能卸载** | 设计如此，反绕过 |
| Inline Hook | 难 | 必须 quiesce 所有线程；MinHook 有检查 |
| ptrace 注入的 .so | 难（如果改了内存）| dlclose + 清理注入痕迹 |
| eBPF | 完美 | close fd / detach |
| VEH Hook | 完美 | `RemoveVectoredExceptionHandler` |
| Git hook | 完美 | 删文件 / `--no-verify` |
| Claude Code hook | 完美 | 改 settings.json |

---

## 6. 横向对比：常见任务的多技术对照

### 6.1 "我要监控所有文件打开"

| 平台 | 自家进程 | 别人的进程 | 全系统 |
|---|---|---|---|
| **Linux** | LD_PRELOAD `open`/`openat` | uprobe + eBPF | eBPF tracepoint `sys_enter_openat` |
| **Windows** | IAT Hook `CreateFileW` | DLL 注入 + Inline Hook | minifilter driver |
| **macOS** | DYLD_INSERT_LIBRARIES（被严格限制）| dtrace（SIP 限制） | EndpointSecurity framework |

### 6.2 "我要给所有 HTTP 请求加 trace ID"

| 技术栈 | 方案 |
|---|---|
| Node.js | `AsyncLocalStorage` + monkey patch `http.request` |
| Python (sync) | `requests.Session` + adapter / contextvars |
| Python (async) | `contextvars` + httpx event hooks |
| Java | Servlet filter + ThreadLocal / OpenTelemetry agent (bytecode instrumentation) |
| Go | context.Context + middleware |
| 跨语言 | OpenTelemetry SDK（标准化）|

### 6.3 "我要确保 commit 前格式化"

| 信任级别 | 方案 |
|---|---|
| 个人习惯 | IDE format-on-save |
| 个人 + 防忘 | 本地 pre-commit hook |
| 团队共享 | pre-commit framework / Husky + `core.hooksPath` |
| 强制 | CI required check + 拒绝合并 |
| 服务端强制 | Git server-side pre-receive 拒收未格式化 commit |

### 6.4 "我要拦截某个函数"

| 谁的函数？ | 推荐 |
|---|---|
| 自己 Python 类的方法 | 装饰器 |
| 三方 Python 模块函数 | `unittest.mock.patch` |
| 三方 Node 库函数（CJS）| monkey assign + `require.cache` |
| C 标准库（自己启动）| `LD_PRELOAD` |
| Win32 API（自己进程，且通过 IAT 调）| IAT Hook |
| Win32 API（动态加载 / 不走 IAT） | Inline Hook (MinHook) |
| Linux 内核函数 | eBPF kprobe / fentry |
| Linux 用户态库函数（任意进程） | eBPF uprobe |
| Qt 控件方法 | 子类化 + override |
| React 组件渲染 | 不要 hook，重构组件 |

---

## 7. 反模式 / 不要这样做

| 反模式 | 为什么不行 | 改用 |
|---|---|---|
| 给 hot path 函数装 ptrace breakpoint | 每次中断 ~微秒级，~10^4/s 就跑满 | eBPF uprobe（10^7/s）|
| 给 `malloc` 装 Inline Hook 做 leak 检测 | 嵌入回调到 hot path 性能差 | LD_PRELOAD + 简单计数 / valgrind |
| 给 React 组件用 monkey patch | 破坏 reconciliation，无意义 | 重构 / 自定义 hook |
| 在 Python `sys.settrace` 里做 IO | 慢到不可用 | `sys.monitoring`（3.12+）按需触发 |
| 在内核 hook 关键路径里 sleep | 死锁 / 调度异常 | 用 work queue 异步 |
| Win32 用 Inline Hook 改系统 DLL 给所有应用 | PatchGuard / Code Integrity 拒绝 / EDR 报警 | API 提供方机制（minifilter/WFP/ETW） |
| 用 LD_PRELOAD 防绕过 / 做安全 | 用户改 env 就绕过 | seccomp / LSM |
| 在 Git 服务端 hook 写很慢的代码 | push 卡住、超时 | 异步 webhook + CI |
| 在 Claude Code Stop hook 写 push/deploy 命令 | 任意触发 = 失控 | 显式人工触发 |
| 在 Python `sys.audit` 里 raise | 不能阻止操作（设计如此）| 用 LSM / seccomp |

---

## 8. 工程化检查清单

不论用哪种 hook，部署前对照：

- [ ] **可观测**：hook 触发有日志（带时间戳、调用方、参数摘要）。
- [ ] **可禁用**：能通过环境变量 / 配置开关 / 删配置文件关掉，无需重启服务。
- [ ] **可卸载**：卸载流程无副作用；如果是 Inline Hook，验证过 quiesce 逻辑。
- [ ] **可降级**：hook 内部异常不影响目标程序主流程（除非有意 block）。
- [ ] **重入安全**：handler 不会因为自己触发自己而死循环。
- [ ] **线程安全**：共享状态有锁或 atomic。
- [ ] **超时保护**：hook 不会无限挂起目标（特别是外部命令型 hook）。
- [ ] **版本兼容**：列出测试过的目标版本范围；变更触发回归。
- [ ] **权限文档化**：装这个 hook 需要什么权限、改了什么、能不能在生产环境用。
- [ ] **替代方案讨论**：在 PR 描述里写"为什么不用更高层抽象解决"。

---

## 9. 法律 / 道德 / 平台政策

| 场景 | 注意 |
|---|---|
| 自家服务器、自家进程 | 随意 |
| 第三方进程的 LD_PRELOAD / DLL 注入 | 看软件 EULA |
| 反作弊 / 安全检测 | 注意国家与平台合规（Android Play Protect 限制内核 hook） |
| 用户键盘 / 屏幕 / 摄像头采集 | 必须明确告知并取得同意（GDPR / 个人信息保护法） |
| 跨进程读写内存 | macOS notarization、Android SELinux 普遍拒绝 |
| 修改系统 DLL / Mach-O / so | 几乎所有现代 OS 都禁止（Code Integrity / SIP / IMA） |

技术本身中立，应用要合规。

---

## 10. 推荐学习路径

如果你刚开始系统学习 hook：

1. **从框架层入门**（最安全）：React/Vue hooks（[01](./01-frontend-hooks.md)）、Qt event filter（[05](./05-qt-hook.md)）。
2. **看懂运行时 hook**：Python monkey patch（[04](./04-language-runtime-hook.md)），理解"为什么改赋值就行"。
3. **理解动态链接**：读 [03](./03-linux-hook.md) §2，跑一遍 `LD_PRELOAD` 替换 malloc。
4. **接触 PE/ELF**：读 [02](./02-windows-api-hook.md) §3，写一次 IAT Hook。
5. **真正下到机器码**：MinHook 跑一次 Inline Hook（[02](./02-windows-api-hook.md) §4）。
6. **走到内核**：先 bpftrace 一行命令，再 BCC，最后 libbpf + CO-RE（[03](./03-linux-hook.md) §5）。
7. **工程化**：把所有项目挂上 Git hooks（[07](./07-git-hooks.md)）和 Claude Code hooks（[06](./06-claude-code-hooks.md)）做日常纪律工具。

---

## 11. 进一步阅读（跨主题）

- *Reverse Engineering for Beginners* (Dennis Yurichev) —— 开源，机器码层基础
- *Practical Reverse Engineering* (Bruce Dang) —— Windows 内核、IDA、Inline Hook
- Brendan Gregg 的 BPF 系列博客 —— Linux 内核 hook 的事实标准
- KDAB 的 GammaRay 源码 —— Qt hook 实战的最高样本
- *Pro Git* —— Git hooks 与服务端流程

---

回到 [README.md](./README.md)
