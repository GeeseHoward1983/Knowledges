# 06 · Claude Code Hooks

> 本篇覆盖 **Claude Code**（Anthropic 出品的官方 CLI 编码助手）的 hook 系统：在 Claude 调用工具前后、会话开始/结束、用户提交消息等关键点上自动执行你的脚本。
> 这套机制让 Claude Code 从"被动按指令执行"变成"主动有纪律的工程伙伴"——比如每次写代码后自动跑 lint、提交前强制跑测试、危险命令前要二次确认、Claude 停止响应时自动通知你。
> 假设读者：用过 Claude Code（`claude` CLI 或 Cursor / VSCode 集成版本），知道它能执行 Bash、Edit、Write 等工具。

---

## 1. 概念

### 1.1 为什么需要 hooks

LLM 的输出本质是概率采样。即使在提示里写"完成后请跑 npm test"，LLM 仍可能：
- 忘记跑。
- 跑了但只截一段输出说"通过了"，实际失败。
- 跑了正确工具但用了错参数。

**Memory / 系统提示 / CLAUDE.md 不能保证执行**——它们只影响 LLM 自己生成的下一段文本。要让"工具调用前/后必然发生某件事"，必须把这件事从 LLM 推理流程中**移出去**，交给执行环境（harness）来跑。这就是 hooks 的定位。

> 一句话：**memory 改变 LLM 想做什么，hooks 改变环境必然做什么。**

### 1.2 hook 在 Claude Code 中的位置

Claude Code 工作循环：

```
用户消息
  ↓
┌─ UserPromptSubmit hook ─┐    ← 在 LLM 看到之前可改写/拒绝
↓
LLM 生成响应（可能含 tool_use）
  ↓
┌─ PreToolUse hook ───────┐    ← 工具调用前；可阻止、可改参数
↓
工具实际执行（Bash/Edit/Write/...）
  ↓
┌─ PostToolUse hook ──────┐    ← 工具调用后；拿到结果，可注入额外上下文
↓
LLM 继续生成
  ↓
（如果没有新的 tool_use）响应结束
  ↓
┌─ Stop / SubagentStop hook ┐  ← 一个回合结束
```

外加几个非循环内的事件：

```
启动时         → SessionStart hook
后台被通知唤起 → Notification hook
压缩历史前后   → PreCompact / PostCompact
会话结束       → SessionEnd hook
```

### 1.3 适用场景一览

| 场景 | hook 类型 | 命令示例 |
|---|---|---|
| 编辑 .ts 后自动 prettier | PostToolUse(Edit) | `npx prettier --write "$file_path"` |
| 写文件前禁止改 .env | PreToolUse(Write) | 拦截 `.env` 路径 |
| Bash 危险命令二次确认 | PreToolUse(Bash) | 正则匹配 `rm -rf`、`sudo` 等 |
| 每次会话开始注入 git 状态 | SessionStart | 输出 `git status -sb` 给 LLM |
| Claude 停止响应时桌面通知 | Stop | 发 toast / 邮件 / Slack |
| 用户输入前自动展开 `@file` | UserPromptSubmit | 改写 prompt 加入文件内容 |
| 提交前强制运行测试 | PreToolUse(Bash) + 匹配 `git commit` | 跑 `npm test` 失败则 block |

---

## 2. 配置位置与优先级

### 2.1 三层 settings.json

Claude Code 按以下顺序合并配置（后者覆盖前者）：

| 层级 | 路径（Windows） | 路径（Linux/macOS） | 用途 |
|---|---|---|---|
| 用户全局 | `%USERPROFILE%\.claude\settings.json` | `~/.claude/settings.json` | 所有项目共用 |
| 项目共享 | `<repo>\.claude\settings.json` | `<repo>/.claude/settings.json` | 团队共享（提交到 git）|
| 项目本地 | `<repo>\.claude\settings.local.json` | `<repo>/.claude/settings.local.json` | 个人覆盖（gitignore）|

> Claude Code 还支持企业策略文件（`managed-settings.json`），优先级最高，由 IT 部署。

### 2.2 settings.json 整体结构

```json
{
  "permissions": { ... },
  "env": { "VAR": "value" },
  "model": "sonnet",
  "hooks": {
    "PreToolUse": [ ... ],
    "PostToolUse": [ ... ],
    "Stop": [ ... ],
    "SessionStart": [ ... ],
    "SessionEnd": [ ... ],
    "UserPromptSubmit": [ ... ],
    "Notification": [ ... ],
    "PreCompact": [ ... ],
    "PostCompact": [ ... ],
    "SubagentStop": [ ... ]
  }
}
```

每个事件名下是一个**数组**，元素为 `{ matcher, hooks }`：

```json
"PreToolUse": [
  {
    "matcher": "Bash",
    "hooks": [
      { "type": "command", "command": "scripts/check_bash.sh", "timeout": 10 }
    ]
  },
  {
    "matcher": "Edit|Write",
    "hooks": [
      { "type": "command", "command": "scripts/check_path.sh" }
    ]
  }
]
```

- `matcher` 是工具名的正则（部分事件忽略，如 `SessionStart`）。
- `hooks` 数组里依次执行；任何一个 exit code 非零都会按事件类型采取相应后果（见下）。

---

## 3. 事件详细规格

### 3.1 PreToolUse —— 工具调用前

**触发**：LLM 决定调用某个 tool，但还没真正执行时。

**输入**（stdin JSON）：

```json
{
  "session_id": "abc123",
  "transcript_path": "/path/to/transcript.jsonl",
  "cwd": "/path/to/repo",
  "hook_event_name": "PreToolUse",
  "tool_name": "Bash",
  "tool_input": {
    "command": "rm -rf /tmp/foo",
    "description": "clean up tmp"
  }
}
```

**退出/输出语义**：

| 方式 | 效果 |
|---|---|
| exit 0 + stdout 空 | 允许 tool 正常执行 |
| exit 0 + stdout JSON | 用 JSON 控制行为（见下） |
| exit 2 | **拒绝** tool 调用；stderr 内容作为反馈给 LLM |
| 其他非零 | 报错但不一定拒绝（视设置） |

JSON 输出格式（结构化控制）：

```json
{
  "decision": "block",
  "reason": "rm -rf 被策略禁止；请改用 trash-cli",
  "modifications": {
    "command": "trash /tmp/foo"
  }
}
```

`decision` 取值：
- `"approve"`：跳过用户确认直接放行（适合白名单）。
- `"block"`：拒绝；`reason` 给 LLM。
- 不写 = 默认放行 + 仍走用户权限流程。

### 3.2 PostToolUse —— 工具调用后

**触发**：tool 执行完，结果即将回给 LLM。

**输入**：在 PreToolUse 的基础上加 `tool_response`：

```json
{
  ...
  "tool_name": "Edit",
  "tool_input": { "file_path": "/a/b/c.ts", "old_string": "...", "new_string": "..." },
  "tool_response": { "success": true }
}
```

**用途**：lint/format/类型检查、把额外上下文 inject 给 LLM。

stdout 可输出 JSON：

```json
{
  "additionalContext": "Linter 报告：c.ts 第 42 行 unused import"
}
```

这段文字会作为系统消息插到 LLM 下一次推理的上下文里。

### 3.3 UserPromptSubmit —— 用户消息提交时

**输入**：

```json
{
  "session_id": "...",
  "hook_event_name": "UserPromptSubmit",
  "prompt": "用户原始消息"
}
```

**输出能力**：
- stdout 输出文本 → **追加**到用户消息（不是替换），LLM 会看到合并后的版本。
- exit 2 + stderr → 拒绝用户消息（直接退回，不传给 LLM）。

适合做：
- 自动展开 `@filename` → 读取文件内容塞进去。
- 检测敏感内容（密码、密钥）→ 拒绝并提示。
- 注入"当前时间 / 当前 git 分支"等动态上下文。

### 3.4 Stop —— LLM 停止响应时

**触发**：本轮 Claude 不再调用任何 tool，准备把控制权交还用户时。

**输入**：

```json
{ "session_id": "...", "hook_event_name": "Stop", "stop_hook_active": false }
```

**输出能力**：
- exit 0 + stdout JSON `{"decision":"block", "reason":"..."}`：**强迫 Claude 继续工作**（反馈给它 reason）。
- 普通 exit 0：通过。

经典用法：
- 发桌面通知 `notify-send "Claude done"`。
- 检查 todo 列表是否清空，没清空就 block。
- 跑 `npm test`，失败则 block 并把错误回给 Claude。

注意 `stop_hook_active`——如果是被 hook block 后再次触发的 Stop，标记为 true，防止无限循环。

### 3.5 SessionStart / SessionEnd

**触发**：会话启动 / 退出。

`SessionStart` 输出会被作为额外的"系统上下文"注入到对话起点——可以预先把工程上下文（最近 git log、依赖版本、CI 状态）告诉 Claude。

`SessionEnd` 用来清理、上传日志、停止本地服务等。

### 3.6 PreCompact / PostCompact

**触发**：Claude Code 决定压缩历史以释放上下文空间时。

可以在压缩前把"绝不能丢的内容"持久化（如未提交的关键决策），或在压缩后把摘要落盘。

### 3.7 Notification

**触发**：Claude Code 想引起用户注意时（权限请求、长时间等待等）。常用于桌面通知集成。

### 3.8 SubagentStop

**触发**：子代理（通过 Agent 工具派发的）结束时。和 Stop 类似，但作用域是子代理。

---

## 4. 完整可运行示例

### 4.1 危险 Bash 命令拦截

`scripts/check_bash.sh`：

```bash
#!/usr/bin/env bash
# 输入：stdin JSON
# 退出 2 + stderr 输出 → 阻止
set -euo pipefail

input=$(cat)
cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // ""')

# 黑名单正则
if echo "$cmd" | grep -E '\b(rm\s+-rf\s+/|mkfs|dd\s+if=|:(){:|:&};:)' >/dev/null; then
    echo "命令被策略禁止：$cmd" 1>&2
    exit 2
fi

# 警告但放行（输出 JSON）
if echo "$cmd" | grep -E '\bsudo\b' >/dev/null; then
    jq -n --arg c "$cmd" '{decision:"block", reason:("sudo 命令需要手工执行：" + $c)}'
    exit 0
fi

exit 0
```

`.claude/settings.json`：

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Bash",
        "hooks": [
          { "type": "command", "command": "bash scripts/check_bash.sh", "timeout": 5 }
        ]
      }
    ]
  }
}
```

Windows PowerShell 版本（`scripts/check_bash.ps1`）：

```powershell
$ErrorActionPreference = 'Stop'
$json = [Console]::In.ReadToEnd() | ConvertFrom-Json
$cmd  = $json.tool_input.command

if ($cmd -match 'rm\s+-rf\s+/|mkfs|dd\s+if=') {
    Write-Error "命令被策略禁止：$cmd"
    exit 2
}
exit 0
```

配置：

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Bash",
        "hooks": [
          { "type": "command", "command": "powershell -ExecutionPolicy Bypass -File scripts/check_bash.ps1" }
        ]
      }
    ]
  }
}
```

### 4.2 写文件后自动格式化

`scripts/auto_format.sh`：

```bash
#!/usr/bin/env bash
set -euo pipefail
input=$(cat)
file=$(printf '%s' "$input" | jq -r '.tool_input.file_path // ""')
[[ -z "$file" || ! -f "$file" ]] && exit 0

case "$file" in
    *.ts|*.tsx|*.js|*.jsx|*.json|*.md|*.css)
        npx --no-install prettier --write "$file" 2>/dev/null || true
        ;;
    *.py)
        ruff format "$file" 2>/dev/null || true
        black --quiet "$file" 2>/dev/null || true
        ;;
    *.go)
        gofmt -w "$file" 2>/dev/null || true
        ;;
    *.rs)
        rustfmt "$file" 2>/dev/null || true
        ;;
esac

exit 0
```

配置：

```json
{
  "hooks": {
    "PostToolUse": [
      {
        "matcher": "Write|Edit|NotebookEdit",
        "hooks": [
          { "type": "command", "command": "bash scripts/auto_format.sh", "timeout": 30 }
        ]
      }
    ]
  }
}
```

### 4.3 Stop 时强制 npm test

`scripts/enforce_tests.sh`：

```bash
#!/usr/bin/env bash
input=$(cat)
already=$(printf '%s' "$input" | jq -r '.stop_hook_active // false')
[[ "$already" == "true" ]] && exit 0    # 二次进入直接放行，防死循环

[[ -f package.json ]] || exit 0
grep -q '"test"' package.json || exit 0

if ! npm test --silent 2>/tmp/claude-test.log; then
    err=$(tail -n 30 /tmp/claude-test.log | tr '\n' '\\' | sed 's/\\/\\n/g')
    jq -n --arg r "$err" '{decision:"block", reason:("npm test 失败，请修复：\n" + $r)}'
fi
exit 0
```

```json
{
  "hooks": {
    "Stop": [
      {
        "hooks": [{ "type": "command", "command": "bash scripts/enforce_tests.sh", "timeout": 300 }]
      }
    ]
  }
}
```

> 注意 `Stop` 事件的 matcher 字段被忽略（没有 tool 名概念），直接 `"hooks": [...]`。

### 4.4 SessionStart 注入 git 状态

```json
{
  "hooks": {
    "SessionStart": [
      {
        "hooks": [
          { "type": "command",
            "command": "bash -c 'echo \"=== git status ===\"; git status -sb; echo \"=== recent commits ===\"; git log --oneline -10'" }
        ]
      }
    ]
  }
}
```

stdout 内容会被作为 system message 注入 Claude 的初始上下文。

### 4.5 UserPromptSubmit 自动展开 `@path/to/file`

`scripts/expand_at.sh`：

```bash
#!/usr/bin/env bash
input=$(cat)
prompt=$(printf '%s' "$input" | jq -r '.prompt')

# 找所有 @xxx 引用，把文件内容追加
extra=""
while IFS= read -r path; do
    if [[ -f "$path" ]]; then
        extra+="\n\n--- 内容 of $path ---\n"
        extra+=$(cat "$path")
        extra+="\n--- 结束 $path ---\n"
    fi
done < <(printf '%s' "$prompt" | grep -oE '@[A-Za-z0-9._/\\-]+' | sed 's/^@//')

if [[ -n "$extra" ]]; then
    printf '%s' "$extra"
fi
exit 0
```

配置：

```json
{
  "hooks": {
    "UserPromptSubmit": [
      { "hooks": [{ "type": "command", "command": "bash scripts/expand_at.sh" }] }
    ]
  }
}
```

用户输入 `修一下 @src/foo.ts 的 bug` → hook 把 `src/foo.ts` 全文追加到 prompt。

### 4.6 路径白/黑名单（PreToolUse Write/Edit）

`scripts/check_path.sh`：

```bash
#!/usr/bin/env bash
input=$(cat)
file=$(printf '%s' "$input" | jq -r '.tool_input.file_path // ""')

case "$file" in
    *.env|*.env.*|*secret*|*credentials*)
        echo "禁止修改敏感文件：$file" 1>&2
        exit 2
        ;;
    *node_modules/*|*.git/*|*dist/*|*build/*)
        echo "禁止修改构建产物或依赖目录：$file" 1>&2
        exit 2
        ;;
esac
exit 0
```

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Edit|Write|NotebookEdit",
        "hooks": [{ "type": "command", "command": "bash scripts/check_path.sh" }]
      }
    ]
  }
}
```

---

## 5. matcher 进阶

`matcher` 是 JS 正则字符串（不带斜杠分隔符），匹配 `tool_name`：

| 写法 | 匹配 |
|---|---|
| `"Bash"` | 仅 Bash |
| `"Edit\|Write"` | Edit 或 Write |
| `".*"` 或 `""` | 全部工具（PreToolUse/PostToolUse 时）|
| `"mcp__.*"` | 所有 MCP 工具 |
| `"mcp__github_.*"` | 某个 MCP 服务器下的所有 tool |

某些事件（Stop/SessionStart/Notification 等）**没有 matcher**，因为它们不绑 tool。

---

## 6. 执行环境与输入输出协议

### 6.1 执行环境

- **shell**：Claude Code 在配置的默认 shell 中执行。Windows 上是 Git Bash（POSIX `sh`），不是 cmd 或 PowerShell——POSIX 工具默认可用。
- **CWD**：默认是项目根目录（也是 `cwd` 字段告诉你的路径）。
- **环境变量**：继承 Claude Code 启动时的环境 + `settings.json.env`。
- **超时**：每个 hook 默认 60s，可在 `{ "type":"command", "timeout": <秒> }` 单独设。超时会被 SIGTERM 杀掉。

### 6.2 stdin 输入

所有 hook 都通过 **stdin 收一个 JSON 对象**。共有字段：

```json
{
  "session_id":     "字符串",
  "transcript_path": "/path/to/transcript.jsonl",
  "cwd":            "/path/to/cwd",
  "hook_event_name": "PreToolUse"
}
```

事件特有字段见上文各节。

### 6.3 stdout / exit code 决策表

| exit | stdout | 行为 |
|---|---|---|
| 0 | 空 | 通过（PostToolUse/Stop/etc 都正常继续）|
| 0 | 纯文本 | PreToolUse: 显示给用户但通过；PostToolUse: 作为额外 context；UserPromptSubmit: 追加到 prompt |
| 0 | JSON 对象 | 用 JSON 字段控制：`decision` / `additionalContext` / `modifications` 等 |
| 2 | stderr | **block**——把 stderr 内容反馈给 LLM（PreToolUse 拒调用；UserPromptSubmit 拒消息；Stop 强迫继续）|
| 非 0 且 != 2 | 任意 | 报错但不一定 block（依事件类型）|

### 6.4 多 hook 顺序与短路

- 数组里的 hooks 按顺序执行。
- 任何一个 exit 2 都会终止后续 hook 并触发 block。
- 多个 matcher 块匹配同一 tool 时，全部执行。

---

## 7. 调试 hooks

### 7.1 把 hook 单独跑

每个 hook 命令就是普通可执行，直接 `echo '{...}' | bash scripts/check_bash.sh` 就能调试，无需起 Claude。

### 7.2 看 transcript

`transcript_path` 指向当前会话的 JSONL，里面每个 tool_use / hook 调用都记录。

### 7.3 临时禁用

把 hook 改成 `{ "type": "command", "command": "true" }` 或把整段从 settings.json 注释掉（json 不支持注释，先备份再清空数组）。

### 7.4 日志习惯

hook 里强烈建议先做一行 trace：

```bash
echo "[$(date -Iseconds)] $hook_event_name $tool_name" >> /tmp/claude-hooks.log
```

排错时能立刻知道是不是被触发了。

---

## 8. 安全考虑

> Claude Code 官方文档明确警告：**hooks 完全以你的用户权限自动执行**。当心：

1. **不要在 hook 里执行 untrusted 输入**。`tool_input.command` 由 LLM 生成，hook 收到后**只能匹配/拒绝/记录**，不要 `eval`。
2. **不要在 hook 里 commit / push / 部署**——一旦 LLM 生成的工具调用碰巧匹配，会自动跑。
3. **secrets 不要直接放 hook 命令字符串里**，用环境变量。
4. **超时一定要设**——意外 hang 的 hook 会让 Claude 整个卡死。

---

## 9. 进阶：和 MCP / skills 配合

- `permissions.allow` / `permissions.deny` 是声明式的工具白/黑名单，**比 hook 轻**，能用就先用。
- skills 给 Claude 提供"该怎么做"的知识，**memory 决定 LLM 想做什么**；hook 决定环境**必然**会做什么。三者层级互补。
- Hook 里可以触发 MCP 工具吗？不能直接。但可以让 hook 输出 `additionalContext: "请先调用 mcp__xxx__yyy"` 引导 LLM。

---

## 10. 常见陷阱

| 陷阱 | 表现 | 原因 | 修法 |
|---|---|---|---|
| Windows 上 hook 写成 `cmd /c ...` 但 Claude 用 git bash | 命令未运行 | shell 不匹配 | 用 bash 语法或显式 `powershell -File`/`cmd /c` |
| jq 未安装 | hook 卡住或报错 | 默认 PATH 没 jq | 装 jq 或改用 Python `json.tool` |
| matcher 没用正则转义 | 匹配过宽 | `"Bash."` 把所有以 Bash 开头的 tool 都吃了 | 精确 `"Bash"` 或 `"^Bash$"` |
| 在 hook 内调 Claude 自己 | 死循环 | hook 触发 hook | 设短超时 + `stop_hook_active` 判断 |
| stdout 输出非 JSON 但 Claude 当 JSON | 报 parse 错 | 在 PostToolUse 直接 echo 多行混合 | 要么纯文本要么纯 JSON |
| timeout 用 ms 而非 s | 秒级被当毫秒处理 | 文档单位是秒 | 改正 |
| `tool_input.command` 是 LLM 生成，可能含特殊字符 | 注入风险 | shell 拼接 | 永远走 stdin/参数，不 `eval` |
| settings.json 写错语法 | Claude 启动失败或忽略 hook | json 不能含注释/末尾逗号 | 用 `jq -e . settings.json` 验证 |
| `decision:"block"` reason 是英文 | 用户读不便 | 习惯 | 你想要的语言写 reason，Claude 会照样转给 LLM |
| 全局 settings 把所有项目 hook 都装上 | 不需要时也跑 | 三层未区分 | 项目特有的放 `<repo>/.claude/settings.json` |

---

## 11. 参考资料

- Claude Code 官方文档 *Hooks*：https://docs.claude.com/en/docs/claude-code/hooks
- Claude Code 官方文档 *Settings*：https://docs.claude.com/en/docs/claude-code/settings
- Claude Code 官方文档 *Permissions*：https://docs.claude.com/en/docs/claude-code/iam

下一篇：[07-git-hooks.md](./07-git-hooks.md)
