# 06 · MCP

> 本篇讲 **MCP(Model Context Protocol)**:用标准协议把外部工具/数据源(数据库、Notion、GitHub、浏览器…)接给 Claude Code。
> 覆盖三种传输、`claude mcp add` 全语法、作用域、`.mcp.json`、认证,以及 **Windows 上的 npx 坑**。
> 资料核实 2026-06。

---

## 1. MCP 是什么

MCP 是一个开放协议:让 Claude 通过统一接口调用"MCP 服务器"暴露的工具与资源。装一个 MCP 服务器,Claude 就多一组能力——读你的数据库、查 Notion、操作 GitHub、驱动浏览器等。

```
Claude Code ──MCP 协议──► MCP 服务器 ──► 真实世界
  (客户端)                 (适配器)        (DB/API/浏览器…)
```

> 直觉:**MCP 之于 Claude,像 USB 之于电脑——一个标准口,插什么用什么。**

---

## 2. 三种传输方式

| 传输 | 用途 | 状态 |
|---|---|---|
| **stdio** | 本地进程,经标准输入输出通信(本地文件、数据库、自写脚本) | 常用 |
| **HTTP** | 远程 URL(streamable HTTP) | **当前标准** |
| **SSE** | 远程,Server-Sent Events | **已弃用**,新服务一律用 HTTP |

> 远程服务器**优先选 HTTP**;遇到 `--transport sse` 连不上,多半改成 `--transport http`、同一 URL 即可。

---

## 3. `claude mcp add` 语法

**铁律:所有 flag(`--transport`/`--scope`/`--env`/`--header`)必须在 `<name>` 之前;`--` 分隔 name 与"服务器自己的启动命令"。**

```bash
# stdio(本地进程)—— 注意 -- 后面是启动该 server 的命令
claude mcp add [options] <name> -- <command> [args...]
claude mcp add --env AIRTABLE_API_KEY=KEY airtable -- npx -y airtable-mcp-server

# HTTP(远程,当前标准)
claude mcp add --transport http <name> <url>
claude mcp add --transport http notion https://mcp.notion.com/mcp

# SSE(已弃用)
claude mcp add --transport sse asana https://mcp.asana.com/sse
```

常用 flag:

| flag | 作用 |
|---|---|
| `--transport stdio\|http\|sse` | 传输类型(默认 stdio) |
| `--scope local\|project\|user` | 作用域(见 §4) |
| `--env KEY=VAL` / `-e KEY=VAL` | 给 stdio server 传环境变量 |
| `--header "Authorization: Bearer xxx"` | 给远程 server 传认证头 |

```bash
# 带认证头的远程 server
claude mcp add --transport http secure-api https://api.example.com/mcp \
  --header "Authorization: Bearer $MY_TOKEN"
```

---

## 4. 作用域:local / project / user

决定配置存哪、谁能看见:

| 作用域 | flag | 存储位置 | 可见范围 |
|---|---|---|---|
| **local**(默认) | `--scope local` | `~/.claude.json`(按项目键) | 只你、只本项目 |
| **project** | `--scope project` | `.mcp.json`(项目根) | 团队共享(提交 git) |
| **user** | `--scope user` | `~/.claude.json`(全局) | 你的所有项目 |

```bash
claude mcp add --transport http stripe https://mcp.stripe.com               # local(默认)
claude mcp add --transport http sentry --scope project https://mcp.sentry.dev/mcp   # 团队共享
claude mcp add --transport http notion --scope user https://mcp.notion.com/mcp      # 跨项目
```

> **优先级**:同名 server 在多个作用域都存在时,**local > project > user**,且"胜出的那条整体生效,字段不合并"。

---

## 5. `.mcp.json`(项目共享配置)

用 `--scope project` 会写进项目根的 `.mcp.json`,提交 git 后全队一致:

```json
{
  "mcpServers": {
    "claude-code-docs": {
      "type": "http",
      "url": "https://code.claude.com/docs/mcp"
    },
    "playwright": {
      "type": "stdio",
      "command": "npx",
      "args": ["-y", "@playwright/mcp@latest"],
      "env": { "SOME_KEY": "${SOME_KEY}" }
    }
  }
}
```

> ⚠️ **项目级 server 需批准**:从 `.mcp.json` 读到的 server 默认显示 "Pending approval",要在交互会话里确认后才启用(防止克隆别人仓库就被悄悄挂上工具)。

---

## 6. 管理子命令

| 命令 | 作用 |
|---|---|
| `claude mcp list` | 列出所有 server(显示 connected / pending / failed) |
| `claude mcp get <name>` | 看某 server 完整配置 |
| `claude mcp remove <name> [-s scope]` | 删除(可指定作用域) |
| `claude mcp add-json <name> '<json>'` | 用 JSON 直接添加 |
| `claude mcp add-from-claude-desktop` | 从 Claude Desktop 导入已配 server |
| `claude mcp serve` | 把 Claude Code 自身作为 MCP server 暴露 |

会话内用 `/mcp` 查看状态、对**远程 server 做 OAuth 认证**。

---

## 7. Windows 上的关键坑:stdio + npx

> 🪟 **Windows 直接 `-- npx ...` 会报 `Connection closed`**——Windows 无法直接执行 npx。必须用 `cmd /c` 包一层:

```bash
# ❌ Windows 上失败
claude mcp add my-server -- npx -y @some/package
# ✅ Windows 正确写法
claude mcp add my-server -- cmd /c npx -y @some/package
```

其他高频坑:

- **JSON 必须合法**:`.mcp.json` / `~/.claude.json` 不允许尾逗号、漏引号、未转义反斜杠——这类错误常**静默失败**。改完用 JSON 校验器过一遍。
- **stdio 工具在会话开始时发现**:中途加的 stdio server,要**重开会话**才能用上它的工具。
- 连接问题排查更多见 [[91.Claude Code/12-故障排查与技巧.md]]。

---

## 8. 安全

- ❌ **别把密钥写进 `.mcp.json`**(它进 git)。用 `${ENV_VAR}` 占位,真实值放 shell profile / `.env`。
- ❌ 别 add 不可信来源的远程 server。
- MCP 工具受**权限系统**约束:可用 `mcp__<server>__*` 形式在 settings.json 里 allow/deny,见 [[91.Claude Code/04-配置与权限.md]]。

---

## 9. 常见 MCP 服务器一览

| 用途 | 例子 |
|---|---|
| 浏览器自动化 | `@playwright/mcp`(本系列环境即用 Edge + Playwright) |
| 知识/文档 | Notion、`claude-code-docs` |
| 研发协作 | GitHub、Sentry、Asana |
| 支付/业务 | Stripe |
| 数据库 | Postgres、Airtable |

> 装好 MCP 后,这些工具就像内置工具一样被 Claude 调用。
> 下一步:让多个 Claude 并行干活 → [[91.Claude Code/07-Subagents与工作流.md]]。
