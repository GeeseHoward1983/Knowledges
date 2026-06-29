# 10 · IDE 集成 · Cursor

> 本篇讲在 **Cursor**(VS Code 衍生的 AI 编辑器)里用 Claude Code:安装、**自动检测失败时的 VSIX 兜底**、以及和 Cursor **自带 AI** 的共存取舍。
> Cursor 是 VS Code fork,通用功能与 [[91.Claude Code/08-IDE-VSCode.md]] 完全一致,本篇只讲差异。

---

## 1. 定位

Cursor 基于 VS Code,因此 **Claude Code 官方扩展可直接在 Cursor 里安装使用**(同理 Windsurf、VSCodium、Kiro 等 fork)。你同时拥有:

- **Cursor 自带 AI**:Tab 补全、Composer / Agent 等(Cursor 自家的)。
- **Claude Code**:Anthropic 的 agentic CLI/扩展。

> 两套 AI 会**功能重叠**,关键是分工(见 §4)。

---

## 2. 安装(同 VS Code)

1. `Ctrl+Shift+X`(Win/Linux)/ `Cmd+Shift+X`(Mac)打开扩展。
2. 搜 **"Claude Code"**,确认发布者 **Anthropic**,Install。
3. 点 **Spark 图标**打开;`Cmd/Ctrl+N` 新对话。

通用功能(plan 审查、可视化 diff、检查点、@提及带行号)同 [[91.Claude Code/08-IDE-VSCode.md]]。

---

## 3. 自动检测失败 → VSIX 手动安装(关键兜底)

> ⚠️ 已知问题:Claude Code 有时**检测不到 Cursor** 是兼容 IDE,导致扩展装不上。此时用本机已装的 CLI 里**捆绑的 VSIX** 手动安装。

先确认 CLI 装在 `~/.claude/local`,然后在**系统终端**(不是 Claude Code 会话里)执行:

```bash
cursor --install-extension ~/.claude/local/node_modules/@anthropic-ai/claude-code/vendor/claude-code.vsix
```

> 这招对其他 VS Code fork 同样适用(把 `cursor` 换成对应 CLI,见 [[91.Claude Code/11-IDE-Windsurf.md]])。
> 实在装不上:直接装独立 CLI,在 Cursor **集成终端**里跑 `claude` 即可——终端方式在任何编辑器都行。

---

## 4. 与 Cursor 自带 AI 的取舍

| 任务 | 更适合 |
|---|---|
| 行内补全、小范围快速改写 | **Cursor 自带**(Tab / Composer) |
| 跨多文件的端到端任务、调研、重型重构、git 流程 | **Claude Code** |

> 2026 的主流实践**不是二选一,而是组合**:用 Cursor 做编辑与补全,用 Claude Code 接重活(端到端实现、批量任务)。终端 agent 正变得越来越"懂 IDE",两者互补。

---

## 5. 计费

扩展免费,需付费 Anthropic 计划或 API Key(见 [[91.Claude Code/02-登录与API-Key.md]]);与你的 Cursor 订阅相互独立。

> 下一篇:另一个 VS Code 衍生编辑器 → [[91.Claude Code/11-IDE-Windsurf.md]]。
