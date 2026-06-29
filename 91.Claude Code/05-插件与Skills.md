# 05 · 插件与 Skills

> 本篇讲怎么用**插件(plugin)**和 **Skills** 扩展 Claude Code,以及它们与自定义命令、子代理、hooks、MCP 的关系。
> 核心命令:`/plugin`。假设你已会基本使用(见 [[91.Claude Code/03-基本使用与斜杠命令.md]])。

---

## 1. 扩展体系全景

Claude Code 可被五类东西扩展,**插件**只是把它们打包分发的"容器":

```
        ┌──────── plugin(打包分发的容器)────────┐
        │                                          │
   ┌────▼────┐  ┌────────┐  ┌───────┐  ┌────────┐ │
   │ Skills  │  │ agents │  │ hooks │  │  .mcp  │ │
   │(技能)   │  │(子代理)│  │(钩子) │  │ (MCP)  │ │
   └─────────┘  └────────┘  └───────┘  └────────┘ │
        + commands(自定义斜杠命令,旧式)            │
        └──────────────────────────────────────────┘
```

| 组件 | 一句话 | 详见 |
|---|---|---|
| **Skill** | 一个按需加载的工作流(`SKILL.md` + 可选脚本) | 本篇 §2 |
| **command** | 手动触发的 `/命令` 提示词(已并入 Skills) | 本篇 §3 |
| **agent** | 有独立上下文的子代理 | [[91.Claude Code/07-Subagents与工作流.md]] |
| **hook** | 工具前后必然执行的脚本 | [[32.hooks/06-claude-code-hooks.md]] |
| **MCP** | 标准协议接外部工具 | [[91.Claude Code/06-MCP.md]] |

---

## 2. Skills(技能)

Skill 是**模型按需自动加载**的一段专长说明:一个 `SKILL.md`(YAML frontmatter + 正文),可附脚本。当任务匹配 skill 描述时,Claude 自己加载它;也可手动 `/skill-name` 触发。

目录结构:

```
.claude/skills/
└── my-skill/
    └── SKILL.md          # frontmatter: name, description;正文是技能说明
```

```markdown
---
name: release-notes
description: 根据 git log 生成发布说明。当用户要写 changelog/release notes 时使用。
---
读取上一个 tag 到 HEAD 的提交,按 Feature/Fix/Chore 归类,用中文输出 markdown。
```

> 关键:`description` 写得越准,Claude 越能在对的时机自动用上它。`${CLAUDE_SKILL_DIR}` 可引用技能自己的目录(放脚本/模板)。

---

## 3. 自定义斜杠命令(已并入 Skills)

老式自定义命令放 `.claude/commands/*.md`,用 `$ARGUMENTS` / `$1` 取参(见 [[91.Claude Code/03-基本使用与斜杠命令.md]] §8)。

**2026 起 commands 已与 Skills 统一**:

- 两者都生成可 `/调用` 的命令。
- 旧的 `.claude/commands/` 文件**继续可用**,无需改。
- 同名冲突时 **Skill 优先**(如 `commands/review.md` 与 `skills/review/SKILL.md` 并存,用后者)。

> 新写扩展优先用 `skills/`;`commands/` 视为遗留兼容。

---

## 4. 插件是什么

插件 = 把上面那些组件打成一个**带版本、可安装、可分享、可信任**的包,类比"Claude Code 的 npm 包"。一个插件可同时提供:Skills + 自定义命令 + 子代理 + hooks + MCP 服务器。

- **插件(plugin)** = 包;**市场(marketplace)** = 装插件的目录/应用商店。
- 用市场是两步:**先 add 市场(只注册目录,不装东西)→ 再 install 具体插件**。

---

## 5. 市场与安装命令

```bash
# ① 添加一个市场(GitHub 简写最常见)
/plugin marketplace add anthropics/claude-code
/plugin marketplace add owner/repo#v2.0      # 可钉到分支/tag
/plugin marketplace add ./my-local-market    # 本地目录(开发自用)

# ② 安装插件
/plugin install plugin-name@marketplace-name
/plugin install plugin-name                  # 市场唯一时可省略
/plugin install https://github.com/user/plugin-name
/plugin install ./my-local-plugin            # 本地

# ③ 管理
/plugin list                 # 已装列表
/plugin info plugin-name     # 看详情
/plugin remove plugin-name   # 卸载
/plugin enable / disable     # 启停
/plugin validate             # 校验插件结构(开发用)
/plugin marketplace update <市场名>   # 刷新目录拉新版

# ④ 安装后免重启激活
/reload-plugins
```

> 找不到插件时的标准修法:`/plugin marketplace update <市场名>` 刷新,或确认已 `add` 过该市场,再重试 install。

---

## 6. 插件目录结构

```
my-plugin/
├── .claude-plugin/
│   └── plugin.json     # 清单(唯一必需文件):name/version/description/author...
├── skills/             # Skills(推荐,新式)
│   └── my-skill/SKILL.md
├── commands/           # 自定义命令(遗留,仍支持)
├── agents/             # 子代理定义
├── hooks/
│   └── hooks.json      # 事件钩子
├── .mcp.json           # 该插件附带的 MCP 服务器
└── README.md
```

- 唯一必需文件是 `.claude-plugin/plugin.json`;其他组件目录**可被自动发现**。
- 插件装好后被复制到 `~/.claude/plugins/cache/`,并**命名空间化**:其技能/命令以 `/插件名:技能名` 形式调用,避免与你本地的 `.claude/` 撞名,本地配置不受影响。

> ⚠️ 写插件别硬编码绝对路径(装到别人机器会断),用相对路径或 `${CLAUDE_PLUGIN_ROOT}`;别把密钥写进 `.mcp.json`,用环境变量引用。

---

## 7. 作用域与团队分发

| 作用域 | 含义 |
|---|---|
| **user**(默认) | 你所有项目可用 |
| **project** | 写进 `.claude/settings.json`,所有协作者共享 |
| **local** | 仅你、仅本仓库 |
| **managed** | 管理员下发,不可改 |

**团队标准化**:在 `.claude/settings.json` 声明市场与插件(可用 `extraKnownMarketplaces`),提交仓库。队友克隆后启动 Claude Code,信任仓库 → 接受提示即自动装上全套插件——"标准工具集随代码一起到达",不再是 wiki 里的口口相传。

更新:`/plugin marketplace update` 或市场自动刷新;官方市场默认开机自动更新,第三方/本地默认不更新(可用 `FORCE_AUTOUPDATE_PLUGINS=1` 控制)。

---

## 8. 官方与社区市场

- **官方**(`claude-plugins-official`):自动可用。
- **社区**(`anthropics/claude-plugins-community`):经审核、钉到 commit SHA 的第三方插件,需手动 add。
- 仓库 `anthropics/claude-code` 内也带演示市场。

---

## 9. 安全警告

> 🔒 **插件以你的用户权限执行任意代码**——hooks 能跑任意命令、MCP 能联网。**只安装可信来源的插件**,装前看清来源与内容,尤其是含 hooks / MCP 的。

下一步:把外部工具接进来 → [[91.Claude Code/06-MCP.md]]。
