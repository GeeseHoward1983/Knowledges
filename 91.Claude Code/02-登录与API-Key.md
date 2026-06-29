# 02 · 登录与 API Key

> 本篇讲 Claude Code 的四种接入方式——**订阅登录、Anthropic API Key、第三方中转、企业云网关(Bedrock/Vertex)**——怎么配、放哪里、优先级、如何切换与排查。
> Windows 与 Linux/macOS 的环境变量设法都给。
> 假设你已装好(见 [[91.Claude Code/01-安装.md]])。

---

## 1. 四种接入方式总览

Claude Code 本身只是个"壳",必须有一个能调用 Claude 模型的凭证才能干活:

| 方式 | 关键配置 | 计费 | 典型场景 |
|---|---|---|---|
| **订阅登录** | `claude` / `/login` 走 OAuth | 包月固定 | 个人日常,最省心 |
| **API Key** | `ANTHROPIC_API_KEY` | 按 token 用量 | 重度、可编程、团队 |
| **第三方中转** | `ANTHROPIC_BASE_URL` + `ANTHROPIC_AUTH_TOKEN` | 看服务商 | 用代理/中转网关 |
| **企业云网关** | `CLAUDE_CODE_USE_BEDROCK` / `CLAUDE_CODE_USE_VERTEX` | 走云账单 | AWS/GCP 合规部署 |

> 一句话:**普通个人选订阅登录;要可控/可编程选 API Key;有中转/合规需求才碰后两种。**

---

## 2. 订阅登录(Pro / Max)

最简单。装好后直接:

```bash
claude
```

首次运行自动弹浏览器做 **OAuth** 授权,用你的 Claude Pro / Max 账号登录即可,**无需管理任何 key**。会话内也可随时:

```
/login     # 重新登录 / 切换账号
/logout    # 退出登录
/status    # 查看当前登录身份与额度
```

凭证存于 `~/.claude/`(Windows `%USERPROFILE%\.claude\`)。

---

## 3. Anthropic API Key

来自 Anthropic **Console** 账户(console.anthropic.com),按 token 计费,适合重度或需要稳定可编程额度的人。

### 3.1 设置环境变量

**Linux / macOS**(写进 `~/.bashrc` / `~/.zshrc` 持久化):

```bash
export ANTHROPIC_API_KEY="sk-ant-xxxxxxxx"
```

**Windows PowerShell**(持久化到用户环境变量):

```powershell
[Environment]::SetEnvironmentVariable("ANTHROPIC_API_KEY", "sk-ant-xxxxxxxx", "User")
```

设完开新终端。验证:`claude doctor` 会显示当前认证来源。

### 3.2 放进 settings.json 的 env(可选)

也可写进**用户级** settings.json(见 [[91.Claude Code/04-配置与权限.md]]):

```json
{ "env": { "ANTHROPIC_API_KEY": "sk-ant-xxxxxxxx" } }
```

> ⚠️ **千万别**把 key 写进**项目级** `.claude/settings.json`——它会被 git 提交导致泄露。要么放系统环境变量,要么放用户级 `~/.claude/settings.json`(不进仓库)。

---

## 4. 第三方中转 / 代理网关

很多人用中转服务(自建代理或第三方网关)访问 Claude。原理:把请求改指到中转地址,并用中转给的 Token 鉴权。

```bash
# Linux / macOS
export ANTHROPIC_BASE_URL="https://your-gateway.example.com"
export ANTHROPIC_AUTH_TOKEN="your-gateway-token"
```

```powershell
# Windows PowerShell
[Environment]::SetEnvironmentVariable("ANTHROPIC_BASE_URL", "https://your-gateway.example.com", "User")
[Environment]::SetEnvironmentVariable("ANTHROPIC_AUTH_TOKEN", "your-gateway-token", "User")
```

- `ANTHROPIC_BASE_URL`:把 API 请求路由到中转地址。
- `ANTHROPIC_AUTH_TOKEN`:作为 `Authorization: Bearer` 头发送(中转服务的鉴权)。

> ⚠️ **实战坑——中转的模型权限不全**:有些中转 Token **只开放部分模型**(例如无 `haiku` 权限)。一旦 Claude Code 内部某个动作默认派发到不可用模型(子代理常默认用 haiku),就会整批 **403** 而**静默失败**——表现为"任务返回空结果",而非报错。
> 排查:看是否 403;必要时用 `/model` 或 `ANTHROPIC_MODEL` 显式指定一个你有权限的模型。详见 [[91.Claude Code/12-故障排查与技巧.md]] 与 [[91.Claude Code/07-Subagents与工作流.md]]。

---

## 5. 企业云网关:Bedrock / Vertex

企业常要求走自家云账户(合规、计费统一)。Claude Code 支持 Amazon Bedrock 与 Google Vertex AI。

**Amazon Bedrock**:

```bash
export CLAUDE_CODE_USE_BEDROCK=1
export AWS_REGION="us-east-1"
# 另需配好 AWS 凭证(aws configure / 环境变量 / IAM 角色)
```

**Google Vertex AI**:

```bash
export CLAUDE_CODE_USE_VERTEX=1
export CLOUD_ML_REGION="us-east5"
export ANTHROPIC_VERTEX_PROJECT_ID="your-gcp-project"
# 另需 GCP ADC: gcloud auth application-default login
```

可配合 `ANTHROPIC_MODEL`(主模型)与 `ANTHROPIC_SMALL_FAST_MODEL`(轻量快速模型,用于子代理等)指定云上对应的 model id。

> 这两条路的变量名/区域要求随版本与云侧调整,**以官方"Bedrock / Vertex 部署"文档为准**;企业环境一般由平台团队统一下发。

---

## 6. 配置位置与优先级

同一台机器上凭证可能来自多处,Claude Code 的取用优先级(高 → 低)大致是:

```
显式环境变量 (ANTHROPIC_API_KEY / ANTHROPIC_AUTH_TOKEN)
        │   ← 会"压住"OAuth 登录
        ▼
settings.json 的 env 块
        │
        ▼
OAuth 订阅登录凭证 (~/.claude/)
```

> 重要后果:如果你又登录了订阅、又设了 `ANTHROPIC_API_KEY`,**API Key 会胜出**(走 Console 计费而非订阅)。想用订阅就**清空** `ANTHROPIC_API_KEY`/`ANTHROPIC_AUTH_TOKEN`,别只设空字符串。用 `claude doctor` / `/status` 确认当前到底在用哪条。

---

## 7. 安全存放清单

- ✅ 系统/用户环境变量,或用户级 `~/.claude/settings.json` 的 `env`。
- ✅ 用 `.env` + 环境注入,密钥不进版本库。
- ❌ 别写进项目级 `.claude/settings.json`(会进 git)。
- ❌ 别写进 `.mcp.json`(项目共享,会进 git;见 [[91.Claude Code/06-MCP.md]])。
- ❌ 别贴进 CLAUDE.md(它是给 LLM 读的记忆文件)。

---

## 8. 切换与排查速查

| 想做什么 | 怎么做 |
|---|---|
| 看当前用哪种认证 | `claude doctor` 或 `/status` |
| 切换订阅账号 | `/login` |
| 退出登录 | `/logout` |
| 临时换模型 | 会话内 `/model`,或启动加 `--model opus` |
| 从订阅切到 API Key | 设 `ANTHROPIC_API_KEY` 后重开终端 |
| 从 API Key 切回订阅 | **清空** `ANTHROPIC_API_KEY` 再 `/login` |

> 认证通了,下一步学怎么用 → [[91.Claude Code/03-基本使用与斜杠命令.md]]。
