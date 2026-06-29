# 09 · IDE 集成 · JetBrains

> 本篇讲 Claude Code 在 **JetBrains 全家桶**(IntelliJ IDEA / PyCharm / WebStorm / GoLand / CLion / PhpStorm / RubyMine / Rider 等)里的官方插件:安装、功能、快捷键、与 VS Code 扩展的差异。
> 通用功能(检查点、计划模式等)与 [[91.Claude Code/08-IDE-VSCode.md]] 一致,本篇侧重 JetBrains 特有的点。

---

## 1. 定位:插件编排 CLI + 用 IDE 的 diff

与 VS Code 的**原生图形扩展**不同,JetBrains 插件的形态是:**调度 Claude Code CLI,并借用 IDE 自带的 diff 工具做审查/接受**。工作流相似,但 UI 集成方式不同——更贴近"被很好包装的终端"。

> 因此:**JetBrains 插件需要你先装好独立 CLI**(见 [[91.Claude Code/01-安装.md]]),插件本身不替代 CLI。

---

## 2. 安装

1. 打开 IDE → `Settings/Preferences` → **Plugins** → **Marketplace**。
2. 搜索 **"Claude Code [Beta]"**。
3. 选 **Anthropic PBC** 发布的那个,点 Install。
4. 按提示**重启 IDE** 激活。

前提:**已安装 Claude Code CLI**。

---

## 3. 支持的 IDE

一个插件通吃 JetBrains 系列:

```
IntelliJ IDEA · PyCharm · WebStorm · GoLand
CLion · PhpStorm · RubyMine · Rider · DataGrip ...
```

> 各 IDE 操作一致,差异主要在语言相关的诊断来源(各自的检查器)。

---

## 4. 核心功能

| 功能 | 说明 |
|---|---|
| **原生 diff 审查** | Claude 的改动在 **JetBrains 自带 diff 工具**里展示,逐行 review、接受/拒绝 |
| **选区自动共享** | 你在编辑器里选中的内容**自动**带给 Claude Code 作上下文 |
| **文件引用快捷键** | 插入文件引用:`Cmd+Option+K`(Mac)/ `Ctrl+Alt+K`(Windows/Linux) |
| **诊断实时共享** | 编辑器里的红/黄波浪线(错误、警告)**实时**同步给 Claude Code |

---

## 5. 快捷键

| 操作 | Mac | Windows / Linux |
|---|---|---|
| 插入文件引用 | `Cmd+Option+K` | `Ctrl+Alt+K` |

其余交互(权限模式 `Shift+Tab`、检查点等)与 CLI/通用一致,见 [[91.Claude Code/03-基本使用与斜杠命令.md]]。

---

## 6. 局限(社区反馈)

> 不少用户认为**官方 JetBrains 插件比 VS Code 扩展弱**:更像"把集成终端包了一层",缺少 VS Code 那种丰富的图形交互(如文件上传、便捷的模式切换)。若你重度依赖可视化体验,VS Code 目前体验更好。

---

## 7. 第三方增强:Claude Code with GUI

社区有一个 **"Claude Code with GUI"** JetBrains 插件,以 Anthropic 官方 VS Code 扩展为基底,把类似体验带到 JetBrains:

- `Alt+K` 即时发送选中代码;
- 在 IDE diff 工具里 review/apply;
- 聊天放侧边栏工具窗或编辑器 tab;
- 左栏浏览全部会话。

> 它属第三方,按需选用;安装第三方插件注意来源可信(同 [[91.Claude Code/05-插件与Skills.md]] §9 的安全提醒)。

---

## 8. 选择建议

| 你的偏好 | 建议 |
|---|---|
| 已在 JetBrains 生态、能接受偏终端的体验 | 官方 "Claude Code [Beta]" |
| 想要接近 VS Code 的图形体验 | 第三方 "Claude Code with GUI",或直接在 IDE 集成终端跑 `claude` |
| 重度可视化需求 | 考虑 VS Code → [[91.Claude Code/08-IDE-VSCode.md]] |

> 下一篇:VS Code 衍生编辑器 → [[91.Claude Code/10-IDE-Cursor.md]]。
