# 07 · Git Hooks

> 本篇覆盖 **Git hooks** 的全套机制：客户端本地钩子（pre-commit、prepare-commit-msg、commit-msg、post-commit、pre-rebase、post-checkout、post-merge、pre-push 等）、服务端钩子（pre-receive、update、post-receive、post-update、reference-transaction）、**钩子框架**（pre-commit、Husky、lefthook、Lefthook、Git LFS）、Git 2.9+ 的 `core.hooksPath` 配置（团队共享钩子的关键）、`--no-verify` 绕过、CI 与本地钩子的协同。
> 假设读者：会用 git，了解 add/commit/push/rebase 的语义。

---

## 1. 概念

### 1.1 什么是 Git Hook

Git 在执行特定操作（commit、push、merge、checkout 等）的**关键节点**会查看 `$GIT_DIR/hooks/` 目录下有没有同名的可执行文件——如果有就调用它。这就是 git hook。

最朴素的"hook"：

```bash
cd your-repo/.git/hooks/
ls
# applypatch-msg.sample
# commit-msg.sample
# fsmonitor-watchman.sample
# post-update.sample
# pre-applypatch.sample
# pre-commit.sample
# pre-merge-commit.sample
# pre-push.sample
# pre-rebase.sample
# pre-receive.sample
# prepare-commit-msg.sample
# push-to-checkout.sample
# update.sample
```

把 `.sample` 后缀去掉，加可执行权限，文件就生效。

### 1.2 hook 文件本身

- 任何可执行文件（脚本带 shebang，或 PE/ELF 二进制）都行。
- 当前工作目录是仓库根（不是 `.git/hooks`）。
- 输入/输出契约因 hook 类型而异（见 §2）。
- 退出码 0 = 通过；非 0 = 中断当前操作（如 commit 被取消）。
- **stdin/stdout 不一定能用**——某些 GUI 客户端可能丢弃 stderr，所以不要依赖输出。

### 1.3 hook 的几个根本限制

1. **本地 hook 默认不进 git 仓库**：`.git/hooks/` 不被 git track（`.git/` 整个被忽略）。所以**钩子默认不能跟项目走**，得用 §6 的方法分发。
2. **`--no-verify` 一键绕过**：`git commit --no-verify` / `git push --no-verify` 跳过相应 hook。所以 hook 不是安全机制，是**纪律工具**。真正的强制约束放服务端。
3. **GUI 不一定执行所有 hook**：少数老 GUI（特别是直接操作 `.git` 而不 fork `git` 命令的）会跳过 hook。现代 IDE / git client（VSCode、JetBrains、SourceTree、GitKraken、git CLI）正常调。

---

## 2. 客户端 hook 全集

下面按"被触发的操作"分组列。Git 官方 `githooks(5)` 是权威。

### 2.1 commit 周期

```
git commit
  │
  ├─ pre-commit            ← 没有参数；stdin/stdout 不用；exit ≠ 0 取消 commit
  │
  ├─ prepare-commit-msg    ← 参数：临时消息文件路径 [, 来源, 来源对象]
  │                          ← 来源 ∈ {message, template, merge, squash, commit}
  │                          ← 用于程序化修改默认 commit message
  │
  ├─ (用户编辑器写消息)
  │
  ├─ commit-msg            ← 参数：消息文件路径；可修改或拒绝
  │
  ├─ post-commit           ← commit 完成后；不能取消（已经 commit 了）
```

| Hook | 典型用途 |
|---|---|
| `pre-commit` | lint / 格式化 / 类型检查 / 跑单元测试 / 拒绝 TODO/FIXME |
| `prepare-commit-msg` | 自动追加 Jira ID（从分支名解析）/ 模板填充 |
| `commit-msg` | 强制 conventional commits 格式 / 拒绝 fixup commit |
| `post-commit` | 触发 watcher / 发通知 / 本地索引更新 |

### 2.2 merge 周期

```
git merge
  │
  ├─ pre-merge-commit  ← 在自动 merge 成功且不需要冲突解决时
  ├─ post-merge        ← 完成后；参数：is_squash (0/1)
```

`post-merge` 经典用法：检测 `package.json`/`requirements.txt` 是否变化，自动 `npm install` 或 `pip install -r requirements.txt`。

```bash
#!/usr/bin/env bash
# .git/hooks/post-merge
changed=$(git diff-tree -r --name-only HEAD@{1} HEAD)
echo "$changed" | grep -q '^package\.json$' && npm install
echo "$changed" | grep -q '^requirements\.txt$' && pip install -r requirements.txt
```

### 2.3 push / fetch

```
git push
  │
  ├─ pre-push   ← 参数：remote 名 + remote URL；stdin：每行 "<local_ref> <local_sha> <remote_ref> <remote_sha>"
```

典型：

```bash
#!/usr/bin/env bash
# .git/hooks/pre-push —— 禁止 push 到 main
remote="$1"; url="$2"
while read local_ref local_sha remote_ref remote_sha; do
    if [[ "$remote_ref" == "refs/heads/main" ]]; then
        echo "禁止直接 push 到 main，请走 PR" 1>&2
        exit 1
    fi
done
```

### 2.4 checkout / rebase

| Hook | 触发 | 参数 |
|---|---|---|
| `post-checkout` | `git checkout` 后 | prev_head, new_head, branch_checkout_flag (0/1) |
| `post-rewrite` | `git rebase` / `git commit --amend` 后 | 命令名（rebase / amend）；stdin 列出被重写的 commit pair |
| `pre-rebase` | `git rebase` 前 | upstream [, branch] |

`post-checkout` 常用：

- 切到不同分支后清缓存（`rm -rf node_modules/.cache`）。
- 自动重启 dev server。
- 检查是否需要 `git lfs pull`。

### 2.5 patch / apply 系列

`applypatch-msg` / `pre-applypatch` / `post-applypatch`：用于 `git am`（从邮件应用 patch）流程，现代 GitHub 工作流很少用。

### 2.6 sparse / worktree（Git 2.30+）

| Hook | 用途 |
|---|---|
| `post-index-change` | index 变化后 |
| `fsmonitor-watchman` | 文件系统监控（Watchman 集成）|

### 2.7 完整客户端 hook 表

| Hook | 阶段 | exit ≠ 0 后果 | 入参 | stdin |
|---|---|---|---|---|
| `applypatch-msg` | git am 应用前 | 中止 | 消息文件 | — |
| `pre-applypatch` | git am 应用后、commit 前 | 中止 | — | — |
| `post-applypatch` | git am commit 后 | 仅告警 | — | — |
| `pre-commit` | commit 前 | 取消 commit | — | — |
| `pre-merge-commit` | 自动 merge 后、commit 前 | 取消 commit | — | — |
| `prepare-commit-msg` | 编辑器打开前 | 取消 commit | msg_file [, source [, sha]] | — |
| `commit-msg` | 用户编辑消息后 | 取消 commit | msg_file | — |
| `post-commit` | commit 完成后 | 仅告警 | — | — |
| `pre-rebase` | rebase 开始前 | 取消 rebase | upstream [, branch] | — |
| `post-checkout` | checkout 完成后 | 仅告警 | prev_head, new_head, is_branch | — |
| `post-merge` | merge 完成后 | 仅告警 | is_squash | — |
| `pre-push` | push 前 | 取消 push | remote, url | ref 信息 |
| `pre-receive` | 服务端：接收前 | 全部拒绝 | — | ref 信息 |
| `update` | 服务端：每 ref 一次 | 拒绝该 ref | ref, old, new | — |
| `post-receive` | 服务端：接收后 | 仅告警 | — | ref 信息 |
| `post-update` | 服务端：post-receive 之后 | 仅告警 | ref... | — |
| `reference-transaction` | 服务端：每个 ref 操作 | 中止事务 | prepared/committed/aborted | ref 行 |
| `push-to-checkout` | 服务端：推到 checked-out branch | 中止 | new_sha | — |
| `pre-auto-gc` | 自动 gc 前 | 跳过 gc | — | — |
| `post-rewrite` | rebase/amend 后 | 仅告警 | command | 旧→新 sha 对 |
| `sendemail-validate` | git send-email 前 | 取消 | patch 文件路径 | — |
| `fsmonitor-watchman` | 长驻 fsmonitor 协议 | — | 协议特殊 | — |
| `p4-changelist` / `p4-prepare-changelist` / `p4-post-changelist` / `p4-pre-submit` | git-p4 集成 | — | — | — |
| `proc-receive` | 服务端：自定义 receive 协议（Git 2.29+） | 拒绝 | — | — |

---

## 3. 服务端 hook

托管在服务器（裸仓库 / GitHub / GitLab / Gitea）上的 hook，**用户无法用 `--no-verify` 绕过**。这是真正的强制约束。

### 3.1 pre-receive

**触发**：push 上来后、ref 更新前。整批 ref 一次性进 stdin：

```
<old_sha> <new_sha> <ref_name>\n
<old_sha> <new_sha> <ref_name>\n
...
```

exit ≠ 0 → **拒绝整个 push**（所有 ref 都不更新）。

例：禁止任何人直接更新 `refs/heads/main`：

```bash
#!/usr/bin/env bash
while read old new ref; do
    if [[ "$ref" == "refs/heads/main" ]]; then
        echo "main 受保护：请通过 PR 合并" 1>&2
        exit 1
    fi
done
```

例：扫描每个新 commit，禁止 push 包含密钥的代码（简化版）：

```bash
#!/usr/bin/env bash
zero="0000000000000000000000000000000000000000"
while read old new ref; do
    [[ "$new" == "$zero" ]] && continue            # 删 branch
    range="$new"
    [[ "$old" != "$zero" ]] && range="$old..$new"

    if git log -p "$range" | grep -E 'AKIA[0-9A-Z]{16}|-----BEGIN.*PRIVATE KEY-----'; then
        echo "检测到疑似密钥：$ref 被拒绝" 1>&2
        exit 1
    fi
done
```

### 3.2 update

像 pre-receive 但**每个 ref 一次**，exit ≠ 0 只拒绝该 ref，其它 ref 仍正常更新。

参数：`<ref_name> <old_sha> <new_sha>`。

### 3.3 post-receive / post-update

push **完成后**触发。无法取消。常用：
- 触发 CI（webhook、`curl`）。
- 自动部署（裸仓库 + post-receive → checkout 到生产目录）。
- 写日志、发通知。

### 3.4 reference-transaction（Git 2.28+）

更细粒度的 ref 事务 hook，被调用三次：
- `prepared`：事务准备好（还可拒绝）
- `committed`：已应用
- `aborted`：已回滚

每次 stdin 给所有受影响 ref。可用于做"ref 变更审计日志"。

### 3.5 proc-receive（Git 2.29+）

代替 `pre-receive` 处理特殊 ref（如 `refs/for/*` 这种 Gerrit 风格的伪 ref）。Gerrit、Forgejo 等用它实现"push 到 refs/for/main 自动开 PR"。日常项目用不到。

### 3.6 GitHub/GitLab/Gitea 的"hook"

托管平台不允许你写裸的 server-side hook，但都提供等价机制：

- **GitHub**：Branch protection rules + Actions（webhook style）+ required status checks。
- **GitLab**：Push rules（Premium）+ Server hooks（自托管 GitLab CE 可写）+ CI gating。
- **Gitea / Forgejo**：Repository → Settings → Webhooks + 直接的 server-side hooks。
- **Bitbucket**：Pre-receive hooks（Data Center）+ Pull request checks。

---

## 4. hook 入参/输出协议速查

| Hook | 参数 | stdin | 拒绝方式 |
|---|---|---|---|
| pre-commit | — | — | exit ≠ 0 |
| prepare-commit-msg | msg_file [, source [, sha]] | — | 修改 msg_file（或 exit ≠ 0 取消）|
| commit-msg | msg_file | — | 修改 msg_file 或 exit ≠ 0 |
| post-commit | — | — | 不能取消 |
| pre-push | remote, url | ref 行 | exit ≠ 0 |
| pre-receive | — | ref 行 | exit ≠ 0 + stderr 给客户端看 |
| update | ref, old, new | — | exit ≠ 0 |
| post-receive | — | ref 行 | 不能取消 |
| post-rewrite | command | old→new 对 | 不能取消 |
| post-checkout | prev, new, is_branch | — | 不能取消 |
| post-merge | is_squash | — | 不能取消 |

> 关键：**所有 stderr 在 push 时会被显示给推送者**。`pre-receive`/`update` 里 `echo "原因..." 1>&2` 是给用户看的友好错误。

---

## 5. 完整示例

### 5.1 Conventional Commits 强制（commit-msg）

`.git/hooks/commit-msg`：

```bash
#!/usr/bin/env bash
msg_file="$1"
msg=$(head -n 1 "$msg_file")

# 跳过 merge / revert / fixup
case "$msg" in
    Merge*|Revert*|fixup!*|squash!*) exit 0 ;;
esac

re='^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([^)]+\))?!?: .{1,72}$'
if ! [[ "$msg" =~ $re ]]; then
    cat 1>&2 <<EOF
✗ Commit 消息不符合 Conventional Commits 规范。

格式：<type>(<scope>)!: <subject>
type：feat / fix / docs / style / refactor / perf / test / build / ci / chore / revert
subject：1-72 字符，无句号结尾

示例：
  feat(auth): 增加 OTP 登录
  fix(api)!: 改返回格式（breaking change）

当前：$msg
EOF
    exit 1
fi
```

加权限：`chmod +x .git/hooks/commit-msg`。Windows 上 git for windows 会按 shebang 自动选 bash。

### 5.2 pre-commit 跑 lint + test（仅对暂存文件）

```bash
#!/usr/bin/env bash
# 只对 staged 的 JS/TS 文件跑 eslint
files=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(js|ts|jsx|tsx)$' || true)
[[ -z "$files" ]] && exit 0

# eslint --fix 然后把改动重新加回 staged
echo "$files" | xargs npx --no-install eslint --fix
echo "$files" | xargs git add

# 跑相关测试（vitest 的 --related）
if [[ -f vitest.config.ts ]]; then
    npx --no-install vitest related --run $files || exit 1
fi

exit 0
```

### 5.3 pre-push 禁止 push WIP

```bash
#!/usr/bin/env bash
remote="$1"
zero=$(git hash-object --stdin </dev/null)
while read local_ref local_sha remote_ref remote_sha; do
    [[ "$local_sha" == "$zero" ]] && continue   # 删
    range="$local_sha"
    [[ "$remote_sha" != "$zero" ]] && range="$remote_sha..$local_sha"

    if git log "$range" --format='%s' | grep -iqE '^wip\b|^drop\b|^fixup!'; then
        echo "✗ 检测到 WIP/DROP/fixup commit，请先整理：" 1>&2
        git log "$range" --oneline --grep='^WIP\|^DROP\|^fixup' 1>&2
        exit 1
    fi
done
```

### 5.4 prepare-commit-msg 自动加分支号

```bash
#!/usr/bin/env bash
msg_file="$1"
source="$2"
[[ "$source" != "" ]] && exit 0   # 仅在用户从头写时插入

branch=$(git symbolic-ref --short HEAD 2>/dev/null) || exit 0
# 假设分支名形如 PROJ-123-add-foo
if [[ "$branch" =~ ([A-Z]+-[0-9]+) ]]; then
    issue="${BASH_REMATCH[1]}"
    # 如果消息开头没有该 issue 号，插入
    head -n 1 "$msg_file" | grep -q "$issue" || \
        sed -i.bak "1s/^/[$issue] /" "$msg_file" && rm -f "$msg_file.bak"
fi
```

### 5.5 服务端 pre-receive：禁止非 fast-forward + 大文件

```bash
#!/usr/bin/env bash
zero="0000000000000000000000000000000000000000"
MAX_BYTES=$((10*1024*1024))      # 10 MB

while read old new ref; do
    [[ "$new" == "$zero" ]] && continue

    # 1. 拒绝非 fast-forward（如果不是新 branch）
    if [[ "$old" != "$zero" ]]; then
        if ! git merge-base --is-ancestor "$old" "$new"; then
            echo "✗ $ref 非 fast-forward，已拒绝；请先 rebase。" 1>&2
            exit 1
        fi
    fi

    # 2. 扫描每个新对象的大小
    range="$new"
    [[ "$old" != "$zero" ]] && range="$old..$new"
    git rev-list --objects "$range" | awk '{print $1}' | while read sha; do
        size=$(git cat-file -s "$sha" 2>/dev/null || echo 0)
        if (( size > MAX_BYTES )); then
            echo "✗ 对象 $sha 超过 ${MAX_BYTES} 字节" 1>&2
            exit 1
        fi
    done
done
exit 0
```

---

## 6. 让本地 hook "跟项目走"

`.git/hooks/` 不在版本控制里，要让全队共用本地 hook，有三种方案：

### 6.1 core.hooksPath（Git 2.9+，**最推荐**）

把 hook 放到 repo 内（如 `.githooks/`），然后改 `core.hooksPath` 指向它：

```bash
git config core.hooksPath .githooks
```

这条配置可以写进 README 让大家手动跑，或更稳——在 `package.json` 的 `prepare` 脚本里自动设置：

```json
{
  "scripts": {
    "prepare": "git config core.hooksPath .githooks"
  }
}
```

`npm install` 时会自动跑 `prepare`，新 clone 的人安装一次依赖就生效。

`.githooks/pre-commit` 跟普通 hook 文件一样，**记得 `chmod +x`**（git 会记录可执行位）。

### 6.2 框架：Husky（Node 生态最流行）

[**Husky**](https://typicode.github.io/husky) 自动管理 `.husky/` 目录 + 设置 `core.hooksPath`：

```bash
npm i -D husky
npx husky init     # 自动设置 + 生成示例
# 写一个 pre-commit:
echo "npm test" > .husky/pre-commit
```

`npm install` 时 `prepare: "husky"` 自动初始化。

`.husky/pre-commit` 是 shell 脚本：

```bash
#!/usr/bin/env sh
. "$(dirname -- "$0")/_/husky.sh"
npx lint-staged
```

> 注：v9+ 简化了，不再需要 `_/husky.sh` 引导。具体语法以官方文档为准（用 Context7 拉 `/typicode/husky` 取最新）。

### 6.3 框架：lefthook（多语言、Go 写、并行执行）

[**lefthook**](https://github.com/evilmartian/lefthook) 提供 YAML 配置 + 自动并行：

```yaml
# lefthook.yml
pre-commit:
  parallel: true
  commands:
    eslint:
      glob: "*.{js,ts,jsx,tsx}"
      run: npx eslint {staged_files}
    prettier:
      glob: "*.{js,ts,jsx,tsx,md,json,css}"
      run: npx prettier --check {staged_files}
    pytest:
      glob: "*.py"
      run: pytest --quiet
```

```bash
lefthook install
```

优势：YAML 声明、并行执行、glob 过滤、跨平台（Windows 友好），不绑 Node。

### 6.4 框架：pre-commit（Python 出品，最跨生态）

[**pre-commit**](https://pre-commit.com)（项目名就叫 pre-commit）通过 `.pre-commit-config.yaml` 描述一组 hook，自动从 git 仓库 clone 它们到本地缓存，**完全语言无关**：

```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v4.6.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-merge-conflict
  - repo: https://github.com/psf/black
    rev: 24.4.2
    hooks:
      - id: black
  - repo: https://github.com/astral-sh/ruff-pre-commit
    rev: v0.4.10
    hooks:
      - id: ruff
        args: ["--fix"]
  - repo: https://github.com/pre-commit/mirrors-eslint
    rev: v9.5.0
    hooks:
      - id: eslint
```

```bash
pip install pre-commit
pre-commit install         # 设置 .git/hooks/pre-commit 转发
pre-commit run --all-files # 一次性跑全仓
```

特点：
- 巨大的现成 hook 生态（trailing-whitespace、check-yaml、bandit、shellcheck、hadolint……）。
- 自动管理依赖（不污染项目）。
- CI 也能跑同样配置（`pre-commit run --all-files`）保证一致性。

---

## 7. 调试 hook

### 7.1 单独执行

hook 就是普通脚本，可以直接调：

```bash
echo "feat: test" > /tmp/msg
.git/hooks/commit-msg /tmp/msg
echo $?
```

### 7.2 让 git 显示更多信息

```bash
GIT_TRACE=1 git commit -m "..."
# 看 git 调了哪些 hook
```

### 7.3 临时禁用

- 单次：`git commit --no-verify` / `git push --no-verify`
- 临时改名：`mv .git/hooks/pre-commit .git/hooks/pre-commit.bak`
- 全局禁用所有：`git -c core.hooksPath=/dev/null commit ...`

### 7.4 GUI 客户端问题

如果 hook 在命令行能跑、GUI 里跑不动，常见原因：
- GUI 启动时 PATH 不全（找不到 `node` / `python`）。Hook 开头 `source ~/.bashrc` 或写绝对路径。
- GUI 用了非 login shell，缺环境变量。

---

## 8. 安全与最佳实践

| 原则 | 说明 |
|---|---|
| **本地 hook 不是安全控制** | `--no-verify` 一键绕过；强制约束放服务端 |
| **服务端 hook 是最后一道防线** | 密钥扫描、文件大小限制、强制 PR 流程 |
| **快** | pre-commit 跑超过 ~3 秒就会被人 `--no-verify`。只对 staged 文件跑，缓存结果 |
| **可复现** | 团队用同一份 hook 配置（pre-commit / husky / lefthook），不要靠口头约定 |
| **明确错误消息** | hook 失败时 `echo "为什么失败 + 怎么修" 1>&2` |
| **CI 兜底** | 任何本地 hook 检查的内容，CI 都要再跑一遍——本地能绕，CI 必须做 |
| **不依赖 stdin/stdout 的不一致行为** | 某些 GUI 不传 stdout 给用户 |
| **不要在 hook 里阻塞太久** | 后台进程别启动；要后台的用 `nohup ... &` 或单独服务 |
| **不在 hook 里写敏感操作** | 加密、上传、删除——一旦误触发后果严重 |

---

## 9. 常见陷阱

| 陷阱 | 表现 | 原因 | 修法 |
|---|---|---|---|
| Windows 上 hook 没执行 | commit 直接通过 | 文件无 shebang 或被 Windows CRLF | 加 `#!/usr/bin/env bash` + LF 行尾 |
| `.git/hooks/` 不进版本控制 | 同事没装 hook | 设计如此 | 用 `core.hooksPath` |
| hook 极慢 | 大家都 `--no-verify` | 没限制到 staged 文件 | `git diff --cached --name-only` 过滤 |
| pre-commit 修改了文件但没 add | commit 还是旧版本 | 修改后没 `git add` | 在 hook 末尾 `git add <修改文件>` |
| commit-msg hook 编辑 msg 后字符乱码 | UTF-8 BOM 问题 | sed 在某些平台加 BOM | 用 `sed -i` 前先 `--binary` 或换 perl |
| pre-push 收不到 ref | stdin 读取错 | 用了 `read` 不带 IFS | `while IFS=' ' read local_ref local_sha remote_ref remote_sha` |
| GUI 跑 hook 报 "node: command not found" | PATH 缺 | 非交互 shell 不读 .bashrc | hook 头部 `export PATH=...` 或绝对路径 |
| `core.hooksPath` 不生效 | 仍走 `.git/hooks/` | 路径写错或没保存到 local config | `git config --local --list \| grep hooksPath` |
| Husky 安装后没生效 | `chmod +x` 没做 | git 不记录可执行位（Windows） | `chmod +x .husky/*` 然后 commit |
| 服务端 hook stderr 不显示 | 用户看不到原因 | 某些协议过滤 stdout | 总是写 stderr，并加前缀 `remote:` 让 git 客户端高亮 |
| post-commit 里跑慢命令 | 命令行卡住 | hook 是同步的 | 用 `nohup ... &` |
| `git commit --amend` 不触发 commit-msg | 漏检查 | amend 也会触发，但很多 hook 忘了 case | 测试一下 |
| `git rebase` 不触发 pre-commit | 期望被 lint 的提交未 lint | rebase 默认不跑 pre-commit | 加 `git rebase --exec 'pre-commit run'` |

---

## 10. CI 与本地 hook 的协同

**本地 hook 是开发体验，CI 是真理。**

| 检查项 | 本地 hook | CI |
|---|---|---|
| 格式 | pre-commit 自动修 | CI 只检不修，失败 |
| Lint | pre-commit | CI |
| 单元测试 | 可选（慢可跳） | 必须 |
| 集成/E2E 测试 | 不跑 | 必须 |
| 安全扫描 | trufflehog 本地版 | 完整版 |
| 依赖审计 | 偶尔 | 每次 |
| Conventional commit | commit-msg | PR title check |

> 推荐组合：**pre-commit framework + GitHub Actions**。同一份 `.pre-commit-config.yaml`：本地 `pre-commit install` 自动跑；CI 里 `pre-commit run --all-files`。

---

## 11. 参考资料

- Git 官方 `githooks(5)`：https://git-scm.com/docs/githooks
- *Pro Git* 第 8 章 Customizing Git：https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks
- pre-commit 框架：https://pre-commit.com
- Husky：https://typicode.github.io/husky
- lefthook：https://github.com/evilmartian/lefthook
- GitHub Actions 等价物（required status checks）：https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository

下一篇：[08-comparison-and-selection.md](./08-comparison-and-selection.md)
