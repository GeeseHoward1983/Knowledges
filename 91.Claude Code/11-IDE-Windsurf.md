# 11 · IDE 集成 · Windsurf

> 本篇讲在 **Windsurf**(VS Code 衍生的 AI 编辑器)里用 Claude Code:安装、VSIX 兜底、与 Windsurf **自带 AI(Cascade)** 的取舍。
> Windsurf 也是 VS Code fork,集成方式与 [[91.Claude Code/10-IDE-Cursor.md]] 几乎一致,通用功能见 [[91.Claude Code/08-IDE-VSCode.md]]。

---

## 1. 定位

Windsurf 基于 VS Code,**Claude Code 官方扩展可直接安装使用**。你会同时有:

- **Windsurf 自带 AI**:Cascade(Windsurf 的 agent)、补全等。
- **Claude Code**:Anthropic 的 agentic 工具。

二者功能重叠,按 §4 分工。

---

## 2. 安装(同 VS Code)

1. `Ctrl+Shift+X` / `Cmd+Shift+X` 打开扩展。
2. 搜 **"Claude Code"**,确认发布者 **Anthropic**,Install。
3. **Spark 图标**打开;`Cmd/Ctrl+N` 新对话。

通用功能(plan 审查、可视化 diff、检查点、@提及)同 [[91.Claude Code/08-IDE-VSCode.md]]。

---

## 3. 自动检测失败 → VSIX 手动安装

与 Cursor 同样的已知问题:Claude Code 可能检测不到 Windsurf。兜底——在**系统终端**用捆绑 VSIX 手动装:

```bash
windsurf --install-extension ~/.claude/local/node_modules/@anthropic-ai/claude-code/vendor/claude-code.vsix
```

> 仍不行:装独立 CLI(见 [[91.Claude Code/01-安装.md]]),在 Windsurf **集成终端**里跑 `claude`,通用可靠。

---

## 4. 与 Windsurf 自带 AI(Cascade)的取舍

| 任务 | 更适合 |
|---|---|
| 补全、Cascade 引导的流式编辑 | **Windsurf 自带** |
| 跨文件端到端任务、调研、重型重构、git | **Claude Code** |

> 同 Cursor:2026 主流是**组合使用**——编辑器自带 AI 管补全与轻量改动,Claude Code 接重活。

---

## 5. 计费与一点说明

- 扩展免费,需付费 Anthropic 计划或 API Key(见 [[91.Claude Code/02-登录与API-Key.md]]),与 Windsurf 订阅独立。
- Windsurf 的归属在 2025–2026 有变动传闻(被收购等),**不影响** Claude Code 的集成方式——只要它还是 VS Code fork,上述方法就成立。

---

## 6. 四种 IDE 横向对比

| | 形态 | diff | 上手 | 备注 |
|---|---|---|---|---|
| [[91.Claude Code/08-IDE-VSCode.md]] | 原生图形扩展 | 内置并排 diff | 最完善 | 基准 |
| [[91.Claude Code/09-IDE-JetBrains.md]] | 插件编排 CLI | IDE 原生 diff | 偏终端 | 全家桶通用 |
| [[91.Claude Code/10-IDE-Cursor.md]] | VS Code fork,同扩展 | 同 VS Code | 同 VS Code | 与 Composer 共存 |
| Windsurf(本篇) | VS Code fork,同扩展 | 同 VS Code | 同 VS Code | 与 Cascade 共存 |

> 回到导航:[[91.Claude Code/00.总览.md]]。
