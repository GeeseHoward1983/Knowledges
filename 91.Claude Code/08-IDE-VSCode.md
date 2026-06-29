# 08 · IDE 集成 · VS Code

> 本篇讲 Claude Code 在 **VS Code** 里的官方扩展:安装、核心功能、diff 审查、检查点、快捷键,以及扩展与 CLI 的关系。
> VS Code 的扩展是**最完善**的 IDE 集成,本篇也是 [[91.Claude Code/10-IDE-Cursor.md]]、[[91.Claude Code/11-IDE-Windsurf.md]] 的基准(它们是 VS Code 衍生)。

---

## 1. 两种用法

```
VS Code 里用 Claude Code
   ├─ ① 官方扩展(图形界面)  ← 本篇重点,体验最好
   └─ ② 集成终端跑 claude    ← 任何带终端的编辑器都行
```

> 扩展提供原生 GUI(侧边栏、可视化 diff、检查点),终端则是通用兜底。两者可并存。

---

## 2. 安装官方扩展

1. 打开扩展面板:`Ctrl+Shift+X`(Windows/Linux)/ `Cmd+Shift+X`(Mac)。
2. 搜索 **"Claude Code"**。
3. **确认发布者是 Anthropic**(避开仿冒同名扩展)。
4. 点 **Install**。

> 另一条路:先装好 CLI(见 [[91.Claude Code/01-安装.md]]),在 VS Code **集成终端**里跑 `claude`,它会提示你安装配套扩展。
> 装完若没出现,`Developer: Reload Window` 重载,或重启 VS Code。

---

## 3. 核心功能

| 功能 | 说明 |
|---|---|
| **plan 审查** | 接受前可**查看并编辑** Claude 的计划 |
| **auto-accept** | 可开启自动应用编辑,适合信任的流程,边看边实现 |
| **@ 提及带行号** | 从选区 `@` 引用文件的**具体行范围** |
| **对话历史** | 浏览过往会话 |
| **多会话** | 在不同 tab / 窗口开多个对话 |
| **侧边栏可拖动** | 主会话放侧边栏,支线开新 tab;面板可拖到侧边/编辑区/底部,记住你的偏好位置 |

点侧边栏的 **Spark 图标**打开 Claude Code;`Cmd/Ctrl+N` 开新对话。

---

## 4. diff 审查(扩展的杀手锏)

当 Claude 要改文件时,扩展弹出**并排 diff**(原文 vs 提议):

- **接受 / 拒绝 / 让它改**:看完再决定。
- **可直接在 diff 里编辑**提议内容再接受——Claude 会被**告知你做了修改**,不会再假设文件等于它原来的提案。

> 这是"终端纯文字 diff"给不了的体验:可视化、可干预。

---

## 5. 检查点(Checkpoints)

- Claude 在**每次改文件前自动建检查点**。
- 不满意:`Esc` 两次,或 `/rewind`,回滚到任意检查点。
- **独立于 git**——不用先 commit 就能放心回退。

---

## 6. 快捷键

| 操作 | 按键 |
|---|---|
| 打开 Claude Code | 点侧边栏 **Spark 图标** |
| 新对话 | `Cmd/Ctrl+N` |
| 回退检查点 | `Esc` `Esc` |
| 权限模式循环 | `Shift+Tab`(见 [[91.Claude Code/03-基本使用与斜杠命令.md]] §6) |

---

## 7. 扩展与 CLI 的关系(易踩)

> ⚠️ **装了扩展 ≠ 终端有 `claude`**。扩展自带一份**私有 CLI 副本**供其聊天面板用,但**不会**把 `claude` 加入系统 PATH。
> 想在**终端**里直接敲 `claude`,仍需**单独安装独立 CLI**(见 [[91.Claude Code/01-安装.md]])。

---

## 8. 计费

扩展**免费安装**,但要真正能用,需要付费 Anthropic 计划或 API Key(见 [[91.Claude Code/02-登录与API-Key.md]])。

---

## 9. 何时用扩展 vs 终端

| 场景 | 推荐 |
|---|---|
| 日常开发、想看可视化 diff、易回退 | **扩展** |
| 批量/脚本化任务、CI、管道 | **终端 CLI**(`claude -p`) |
| 很多人的实践 | 两者并用:终端跑重活,IDE 做可视化审查 |

> 其他编辑器:[[91.Claude Code/09-IDE-JetBrains.md]]、[[91.Claude Code/10-IDE-Cursor.md]]、[[91.Claude Code/11-IDE-Windsurf.md]]。
