# 04 · 语言运行时层 Hook（Python / Node.js）

> 本篇讲两类动态语言运行时提供的 hook 机制：**Python**（monkey patching、`sys.settrace` / `sys.setprofile`、`sys.audit`、import system 三层、`unittest.mock.patch`、`sys.monitoring`）和 **Node.js**（CommonJS require hook、`Module._load` 改写、ESM Loader hook、`async_hooks` / AsyncLocalStorage、Inspector Protocol）。
> 假设读者：会 Python / JavaScript，了解模块系统基础。

---

## 1. 为什么动态语言的 hook 容易

C/C++ 要 hook 一个函数得改机器码；Python/JS 这样的动态语言里，函数只是一个**对象引用**，"hook" 就是"把那个引用换成自己的函数"。整个 runtime 的结构就是给 hook 留好门的——module、class、function 都是可被替换、可被遍历的字典。

但"容易"不等于"安全"。后面会看到大量陷阱：闭包捕获时机、import 缓存、`async` 上下文丢失、ESM 与 CJS 互操作、stack frame 生命周期……每一类都需要专门的 API 才能正确处理。

---

## 2. Python Hook

### 2.1 Monkey Patching

最朴素的"hook"——直接把模块或类的属性赋一个新对象：

```python
import time

_orig_sleep = time.sleep
def fake_sleep(secs):
    print(f"[hook] sleep({secs}) called")
    _orig_sleep(secs * 0.1)   # 实际只睡 10%

time.sleep = fake_sleep

time.sleep(5)   # 实际只睡 0.5 秒，并打印日志
```

**为什么能工作**：
- `time.sleep` 是 `time` 模块对象的 `__dict__['sleep']`，可写。
- Python 的属性查找是动态的，`time.sleep(5)` 每次都查一次。

**哪里会失败**：

```python
from time import sleep    # 在 import 时把 sleep 绑到本模块的全局
time.sleep = fake_sleep   # 改了 time.sleep
sleep(5)                  # 但本模块的 sleep 仍指向原函数！
```

`from X import Y` 把 `Y` 复制到当前模块的命名空间——之后再改 `X.Y` 影响不到本模块。这是 monkey patching 最常见的失败原因。

**最佳实践**：

1. 改 **属性所在对象**（`time.sleep = ...`），不改 import 它的代码的局部引用。
2. 在测试中用 `unittest.mock.patch`（下一节），它会自动 restore。
3. 避免在生产代码里 monkey patch——不可预测的全局副作用最容易制造长期 bug。

### 2.2 unittest.mock.patch

Python 3 内置的最佳 mock 工具：

```python
from unittest.mock import patch, MagicMock

# 用法 1：装饰器
@patch('mymodule.requests.get')
def test_fetch(mock_get):
    mock_get.return_value.json.return_value = {'ok': True}
    from mymodule import fetch
    assert fetch() == {'ok': True}
    mock_get.assert_called_once()

# 用法 2：上下文管理器
def test_fetch2():
    with patch('mymodule.requests.get') as mock_get:
        mock_get.return_value.json.return_value = {'ok': True}
        from mymodule import fetch
        assert fetch() == {'ok': True}
# 退出 with 块后，mymodule.requests.get 自动恢复
```

**关键陷阱：patch 路径**。

```python
# mymodule.py
import requests
def fetch(): return requests.get('...').json()

# 错误：会 patch 全局 requests.get，但 mymodule 仍持有原引用？
patch('requests.get')   # ❌（在 mymodule 里 requests 是模块对象，所以这里其实也能 work）

# 正确：patch "被使用的位置"
patch('mymodule.requests.get')   # ✓
```

规则：**patch 路径要指向"使用这个名字的地方"**。如果 `mymodule` 用 `from requests import get` 而后调 `get()`，那就要 `patch('mymodule.get')`。

### 2.3 sys.settrace / sys.setprofile —— 解释器级追踪

`sys.settrace(func)` 让 Python 解释器在每一行执行、函数调用、返回、异常时调用 `func`：

```python
import sys

def tracer(frame, event, arg):
    if event == 'call':
        code = frame.f_code
        print(f"call {code.co_filename}:{code.co_firstlineno} {code.co_name}")
    return tracer    # 返回值控制 line-level trace

sys.settrace(tracer)

def f(x):
    return x + 1

f(10)
sys.settrace(None)
```

`event` 可能值：`call` / `line` / `return` / `exception` / `opcode`。

**用途**：
- 代码覆盖率工具（`coverage.py`）。
- 调试器（`pdb` 基于 `settrace`）。
- 性能 profile。

**注意**：
- 极慢——每一行都回调一次 Python 函数，性能损耗 10x-100x。
- `sys.setprofile` 只在 call/return 时回调，开销小得多，适合 profiler。
- 这是 **per-thread** 的（用 `threading.settrace` 给所有新线程也装）。
- 对 C 函数无效（C 实现的内置不走解释器）。

### 2.4 sys.audit —— 审计 hook（Python 3.8+）

```python
import sys

def audit(event, args):
    if event == 'open':
        print(f"AUDIT open: {args}")
    elif event == 'urllib.Request':
        print(f"AUDIT urllib: {args[0]}")

sys.addaudithook(audit)

open('/etc/hosts')           # → AUDIT open: ('/etc/hosts', 'r', 524288)
import urllib.request
urllib.request.urlopen('http://example.com')   # → AUDIT urllib: http://...
```

CPython 在关键位置（文件 IO、网络、subprocess、import、eval、compile）发出审计事件。无法被 Python 代码绕过（除了 `sys.addaudithook` 注册顺序），适合安全监控。

完整事件列表见 [PEP 578](https://peps.python.org/pep-0578/) 和 https://docs.python.org/3/library/audit_events.html。

### 2.5 sys.monitoring —— 低开销监控（Python 3.12+）

3.12 引入的新机制，比 `settrace` 高效得多——只对**感兴趣的事件**插桩，未启用的位置零开销：

```python
import sys
mon = sys.monitoring

TOOL_ID = mon.PROFILER_ID
mon.use_tool_id(TOOL_ID, "my-profiler")
mon.set_events(TOOL_ID, mon.events.PY_START | mon.events.PY_RETURN)
mon.register_callback(TOOL_ID, mon.events.PY_START,
    lambda code, instruction_offset: print('start', code.co_name))
mon.register_callback(TOOL_ID, mon.events.PY_RETURN,
    lambda code, offset, retval: print('return', code.co_name, retval))

def f(): return 42
f()
mon.free_tool_id(TOOL_ID)
```

未来 profiler / coverage 工具都会迁移到这个 API。

### 2.6 Import Hooks —— 拦截 import 语句

Python import 系统分三层：

1. **Finders**（`sys.meta_path`）：决定"这个模块名能从哪儿找"。
2. **Loaders**：决定"怎么把找到的源/字节码变成模块"。
3. **Cache**（`sys.modules`）：已加载模块，import 同名直接命中。

要在 import 时插手，写一个自定义 **MetaPathFinder**：

```python
import sys
import importlib.util
import importlib.abc

class LogFinder(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path, target=None):
        print(f"[import-hook] trying {fullname}")
        return None   # 返回 None 表示"我不处理，让下一个 finder 来"

sys.meta_path.insert(0, LogFinder())

import json   # → [import-hook] trying json
```

更激进的——**完全替换**某个模块：

```python
import sys, importlib.abc, importlib.util, types

class FakeRequestsLoader(importlib.abc.Loader):
    def create_module(self, spec):
        m = types.ModuleType(spec.name)
        m.get = lambda url, **kw: types.SimpleNamespace(
            status_code=200, text="fake", json=lambda: {"fake": True})
        return m
    def exec_module(self, module): pass

class FakeFinder(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path, target=None):
        if fullname == 'requests':
            return importlib.util.spec_from_loader(fullname, FakeRequestsLoader())
        return None

sys.meta_path.insert(0, FakeFinder())

import requests
print(requests.get('http://x').json())   # → {'fake': True}
```

**注意**：要在被 import 的模块**首次**被 import 之前注册 finder——`sys.modules` 缓存命中后就不查 finder 了。

### 2.7 用 typing 静态拦截：装饰器作为 hook

装饰器是 Python 最常见的"显式 hook"机制：

```python
import time
import functools

def timed(fn):
    @functools.wraps(fn)
    def wrapper(*args, **kwargs):
        t0 = time.perf_counter()
        try:
            return fn(*args, **kwargs)
        finally:
            print(f"{fn.__name__}: {time.perf_counter() - t0:.3f}s")
    return wrapper

@timed
def slow():
    time.sleep(1)

slow()   # → slow: 1.003s
```

**装饰器 vs monkey patch**：装饰器是源码侵入式（你必须能改源），monkey patch 是运行时无侵入。装饰器更安全、可读性更好；monkey patch 用于测试或集成第三方。

### 2.8 Python 完整可运行示例：自动重试 hook

业务诉求：把所有 `requests.get` 调用自动包一层"3 次重试 + 指数退避"。

```python
# autoretry.py
import functools
import time
import requests

_orig_get = requests.get

@functools.wraps(_orig_get)
def get_with_retry(url, **kwargs):
    last = None
    for attempt in range(3):
        try:
            r = _orig_get(url, **kwargs)
            if r.status_code < 500:
                return r
            last = RuntimeError(f"HTTP {r.status_code}")
        except (requests.ConnectionError, requests.Timeout) as e:
            last = e
        time.sleep(0.5 * (2 ** attempt))
    raise last

requests.get = get_with_retry
```

用法：

```python
import autoretry          # 装载 hook
import requests
r = requests.get('https://example.com')   # 自动带重试
```

**改进版（线程安全 + 可卸载）**：

```python
import contextlib

@contextlib.contextmanager
def autoretry_enabled():
    orig = requests.get
    requests.get = get_with_retry
    try:
        yield
    finally:
        requests.get = orig

with autoretry_enabled():
    requests.get('https://example.com')
# 退出 with 后 requests.get 恢复
```

---

## 3. Node.js Hook

### 3.1 CommonJS require hook

Node 的 CommonJS 模块系统底层是 `Module._load(request, parent, isMain)`，所有 `require()` 都走它。改写它就能拦截全部 require：

```js
// hook-require.js
const Module = require('module');
const origLoad = Module._load;

Module._load = function (request, parent, isMain) {
  const exported = origLoad.call(this, request, parent, isMain);
  console.log(`[require] ${request} -> ${typeof exported}`);
  return exported;
};
```

```bash
node -r ./hook-require.js app.js
```

`-r` (`--require`) 在加载主模块前先执行指定脚本——所有后续 require 都被 hook 到。

**更精细：替换具体模块**：

```js
const Module = require('module');
const origLoad = Module._load;

Module._load = function (request, parent, isMain) {
  if (request === 'fs') {
    const realFs = origLoad.call(this, request, parent, isMain);
    return new Proxy(realFs, {
      get(target, prop) {
        if (prop === 'readFileSync') {
          return (...args) => {
            console.log('readFileSync', args[0]);
            return target.readFileSync(...args);
          };
        }
        return target[prop];
      }
    });
  }
  return origLoad.call(this, request, parent, isMain);
};
```

### 3.2 monkey patch 单个模块的方法

更轻量的方式——直接改 cache 里的模块导出：

```js
const fs = require('fs');
const orig = fs.readFile;
fs.readFile = function (path, ...rest) {
  console.log('readFile', path);
  return orig.call(this, path, ...rest);
};
```

注意：在 CJS 模块里，`require('fs')` 多次拿到同一个对象（来自 `require.cache`），改一次全程生效。

### 3.3 ESM Loader Hooks（Node 20.6+）

ES Modules 不走 `Module._load`，所以 CJS 的 hook 对 `import` 语句无效。Node 20.6 引入了稳定的 `module.register()` API：

```js
// loader.mjs
import { register } from 'node:module';
import { pathToFileURL } from 'node:url';

register('./hook-loader.mjs', pathToFileURL('./'));
```

```js
// hook-loader.mjs
export async function resolve(specifier, context, nextResolve) {
  console.log('resolve', specifier);
  return nextResolve(specifier, context);
}

export async function load(url, context, nextLoad) {
  const result = await nextLoad(url, context);
  if (url.endsWith('.js')) {
    // 改源码：在每个文件头加一行 console.log
    return {
      ...result,
      source: `console.log('loaded ${url}');\n${result.source}`,
    };
  }
  return result;
}
```

```bash
node --import ./loader.mjs app.mjs
```

钩子三个钩子函数：
- `resolve(specifier, context, nextResolve)`：把模块路径解析为 URL。
- `load(url, context, nextLoad)`：拿到 URL 后实际读取/编译，可返回 `{ source, format, shortCircuit }`。
- 钩子链通过 `nextX()` 调用下一个 loader。

> Node 20 之前的 `--experimental-loader` 已弃用，新代码用 `register()`。

### 3.4 async_hooks / AsyncLocalStorage

JavaScript 是单线程异步的，"当前调用上下文"在 `await`/回调里很容易丢。Node 提供：

```js
const { AsyncLocalStorage } = require('node:async_hooks');
const als = new AsyncLocalStorage();

function handleRequest(req) {
  als.run({ requestId: req.id, user: req.user }, async () => {
    await processStep1();
    await processStep2();
  });
}

async function processStep1() {
  console.log('current request:', als.getStore().requestId);   // ✓ 正确拿到
}
```

底层 API `async_hooks.createHook` 提供更细粒度（每个异步资源创建/销毁/before/after 都回调），但开销大，应用层用 `AsyncLocalStorage` 即可。

**典型用途**：
- 给所有日志自动附加 requestId（不用层层透传 context）。
- 分布式 trace（OpenTelemetry 用它存当前 span）。

### 3.5 Inspector Protocol —— Chrome DevTools 协议

Node 启动加 `--inspect`，开启一个 WebSocket 服务，可以用 Chrome DevTools 协议（CDP）做：

- 断点、单步、查变量。
- 采样 profiler、heap snapshot。
- console 远程访问。

代码里也能用：

```js
const inspector = require('node:inspector/promises');
const session = new inspector.Session();
session.connect();

await session.post('Profiler.enable');
await session.post('Profiler.start');
// ... 跑一段代码
const { profile } = await session.post('Profiler.stop');
require('node:fs').writeFileSync('cpu.cpuprofile', JSON.stringify(profile));
```

可以在 Chrome `chrome://inspect` 里打开 `cpu.cpuprofile` 看火焰图。

### 3.6 Node 完整可运行示例：HTTP 请求审计

目的：拦截所有出站 `http`/`https` 请求，打印 URL 和耗时。

```js
// audit.js
const http = require('node:http');
const https = require('node:https');
const { performance } = require('node:perf_hooks');

function wrap(module, name) {
  const orig = module[name];
  module[name] = function (...args) {
    const url = typeof args[0] === 'string'
      ? args[0]
      : (args[0]?.href || `${args[0]?.protocol}//${args[0]?.host}${args[0]?.path}`);
    const t0 = performance.now();
    const req = orig.apply(this, args);
    req.on('response', (res) => {
      console.log(`[audit] ${name.toUpperCase()} ${url} ${res.statusCode} ${(performance.now()-t0).toFixed(1)}ms`);
    });
    req.on('error', (err) => {
      console.log(`[audit] ${name.toUpperCase()} ${url} ERROR ${err.message}`);
    });
    return req;
  };
}

wrap(http, 'request');
wrap(http, 'get');
wrap(https, 'request');
wrap(https, 'get');
```

```bash
node -r ./audit.js -e 'require("https").get("https://example.com", r => r.resume())'
# [audit] GET https://example.com 200 145.2ms
```

绝大多数 HTTP 客户端库（axios、got、node-fetch）底层都调 `http.request`/`https.request`，所以这一个 hook 就能审计**所有出站请求**。

### 3.7 Node 完整可运行示例：require hook 实现 TypeScript 即时编译

```js
// ts-hook.js
const Module = require('module');
const ts = require('typescript');
const fs = require('fs');

Module._extensions['.ts'] = function (mod, filename) {
  const src = fs.readFileSync(filename, 'utf8');
  const { outputText } = ts.transpileModule(src, {
    compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 },
  });
  mod._compile(outputText, filename);
};
```

```bash
node -r ./ts-hook.js script.ts
```

`Module._extensions` 决定不同扩展名的文件怎么"变成模块"，是 `ts-node` / `@babel/register` 这类工具的核心机制。

---

## 4. 进阶话题

### 4.1 Python：闭包捕获时机

```python
funcs = [lambda: i for i in range(3)]
print([f() for f in funcs])   # → [2, 2, 2]

# 正确
funcs = [lambda i=i: i for i in range(3)]
print([f() for f in funcs])   # → [0, 1, 2]
```

写 hook 时如果在循环里生成多个 wrapper，必须用默认参数固化。

### 4.2 Node.js：CJS 与 ESM 混用

- CJS 文件可以 `require()` CJS，也能 `await import()` ESM。
- ESM 文件可以 `import` ESM 和 CJS（CJS 默认导出在 `default`）。
- CJS `require()` 一个 ESM 模块会**失败**（除非用 `--experimental-require-module`）。
- **CJS 的 `Module._load` hook 不影响 ESM 的 `import`**。要同时拦两边，得装 CJS hook + ESM loader hook 两套。

### 4.3 Python：C 扩展无法被 settrace

`numpy`、`pandas` 内部的 C 函数不会触发 `settrace`/`setprofile`。要 trace 它们需要 native 工具（perf、py-spy 的 native 模式）。

### 4.4 Node.js：性能影响

- `async_hooks.createHook` 启用后**所有 Promise 创建都有开销**，生产环境慎用。
- `AsyncLocalStorage` 在 Node 16+ 用 V8 PromiseHook 内部实现，开销 < 5%，可放心。

### 4.5 Python：sys.monitoring vs settrace

| | settrace | sys.monitoring (3.12+) |
|---|---|---|
| 启用粒度 | 全局 / 按线程 | 按事件类型 + 按代码对象 |
| 性能 | 慢（10x-100x）| 未启用事件零开销 |
| 多工具共存 | ✗（互相覆盖）| ✓（多个 tool ID）|
| 适用 | 老 profiler/调试器 | 新代码首选 |

---

## 5. 调试与卸载

### Python

- `monkey patch` 卸载：保存原引用，需要时赋回。
- `mock.patch`：自动 restore，无需手动。
- `sys.addaudithook`：**无法移除**（PEP 578 设计如此，防绕过）。
- `sys.settrace(None)`：关闭 trace。
- `meta_path` finder：从 `sys.meta_path` 删除；已 import 的模块在 `sys.modules` 里要清掉才会重新走 finder。

### Node.js

- monkey patch：保存原引用，需要时赋回。
- `Module._load` 改写：保存 orig，恢复 `Module._load = orig`。
- `require.cache[require.resolve('x')] = undefined` 强制下次 require 重新加载。
- ESM loader 一旦注册不能撤销（Node 22 仍是如此）。

---

## 6. 常见陷阱

| 陷阱 | 表现 | 原因 | 修法 |
|---|---|---|---|
| Python 用 `from x import y` 后 monkey patch `x.y` | 旧引用仍生效 | import 时已绑定 | patch `当前模块.y` 或改 import 方式 |
| `mock.patch` 路径错 | mock 没生效 | patch 了"原产地"而非"使用地" | 改成 `patch('caller_module.X')` |
| Python settrace 拖慢 100x | 测试很慢 | 每行回调 | 改用 setprofile 或 sys.monitoring |
| 注册 import hook 太晚 | 第一次 import 没被拦 | 模块已在 sys.modules | 在程序入口最早处注册 |
| Node CJS hook 不影响 ESM | import 的模块没被拦 | 两个独立的模块系统 | 加 ESM loader hook |
| `async_hooks.createHook` 影响生产性能 | RPS 下降 | 每个 promise 都触发 | 改用 AsyncLocalStorage |
| Python lambda 在循环里捕获变量 | 全捕获最后一个 | late binding | 默认参数固化 |
| Node monkey patch 后失去 prototype 链 | this 错 | 直接函数赋值未 `bind` | 用 Proxy 或保留 `apply` |
| Python audit hook 性能 | 大量 IO 时变慢 | 每次审计回调都有开销 | hook 内尽快返回，避免重 IO |
| ESM loader 用 CJS 引入 | TypeError | loader 必须是 mjs | 重命名为 .mjs |

---

## 7. 参考资料

### Python
- 官方文档：https://docs.python.org/3/library/sys.html#sys.settrace
- PEP 578（Runtime Audit Hooks）：https://peps.python.org/pep-0578/
- PEP 669（Low Impact Monitoring）：https://peps.python.org/pep-0669/
- *Python Cookbook* 第 9 章（元编程、装饰器、类装饰器）
- `coverage.py` 源码（学习生产级 settrace 使用）

### Node.js
- ESM Loaders: https://nodejs.org/api/module.html#customization-hooks
- AsyncLocalStorage: https://nodejs.org/api/async_context.html
- Inspector Protocol: https://chromedevtools.github.io/devtools-protocol/
- `ts-node` 源码（学习生产级 require hook）

下一篇：[05-qt-hook.md](./05-qt-hook.md)
