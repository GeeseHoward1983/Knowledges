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

---

## 8. JVM 字节码插装（Java）

### 8.1 为什么 Java 能在运行时改代码

JVM 的一个核心设计是"类文件不是一次写死的机器码"——每次 `ClassLoader.loadClass()` 执行时，JVM 接收的是**字节码（`.class` 文件格式）**，由 JIT 编译器在运行时翻译为机器码。`java.lang.instrument` 包正是利用了这个时间窗：在字节码变成机器码**之前**，允许外部代码修改它。

这带来了两条入口：

- **premain（静态 attach）**：JVM 启动时通过 `-javaagent:agent.jar` 加载，在 main 方法执行前完成插装。99% 的 APM 探针、字节码测试框架都走这条路。
- **agentmain（动态 attach）**：对一个**已在运行**的 JVM 进程，通过 Attach API（`com.sun.tools.attach.VirtualMachine`）动态注入 agent。Arthas、JVM 诊断工具走这条路。

### 8.2 Instrumentation API 核心接口

`java.lang.instrument.Instrumentation` 是整个体系的核心，你在 `premain` / `agentmain` 中拿到它的实例：

```java
// 两个 agent 入口，任选其一
public static void premain(String agentArgs, Instrumentation inst) { ... }
public static void agentmain(String agentArgs, Instrumentation inst) { ... }
```

关键方法：

| 方法 | 说明 |
|---|---|
| `inst.addTransformer(transformer, canRetransform)` | 注册字节码变换器；第二参数为 true 时支持后续 retransform |
| `inst.retransformClasses(Class<?>...)` | 对已加载的类**重新触发**所有 transformer（不能改方法签名/字段） |
| `inst.redefineClasses(ClassDefinition...)` | 直接替换已加载类的字节码（更强制，同样不能改签名） |
| `inst.getAllLoadedClasses()` | 枚举 JVM 中所有已加载类 |
| `inst.getObjectSize(obj)` | 获取对象浅尺寸（内存分析用） |

`retransformClasses` 与 `redefineClasses` 的限制：
- 不能增加/删除/改名字段或方法。
- 不能改变方法签名（参数、返回类型）。
- 不能改变类的继承关系。
- **可以**：改方法体内的逻辑（注入探针、修改分支、插入打印）。

### 8.3 ClassFileTransformer

每次类被加载（或 retransform/redefine）时，JVM 依次调用所有注册的 `ClassFileTransformer.transform()`：

```java
import java.lang.instrument.ClassFileTransformer;
import java.security.ProtectionDomain;

public class TimingTransformer implements ClassFileTransformer {
    @Override
    public byte[] transform(
            ClassLoader loader,
            String className,          // 形如 "com/example/MyService"（斜线而非点）
            Class<?> classBeingRedefined,
            ProtectionDomain domain,
            byte[] classfileBuffer     // 原始字节码
    ) {
        if (!className.startsWith("com/example/")) {
            return null;   // null 表示不修改，性能友好
        }
        // 在此修改 classfileBuffer，返回新字节码
        return instrument(className, classfileBuffer);
    }
}
```

返回 `null` 表示"我不修改这个类"，JVM 继续用原字节码——这是性能关键：**对不感兴趣的类尽快返回 null**，避免无效复制。

### 8.4 字节码操作库对比

直接手写字节码指令集极易出错，生产中都用库：

| 库 | 层次 | 特点 | 典型用场 |
|---|---|---|---|
| **ASM** | 低级（visitor 模式） | 最轻量、最快、学习曲线陡 | 框架底层、追求极致性能 |
| **Byte Buddy** | 高级 DSL | 声明式 API、类型安全、易用 | APM 探针、运行期代理 |
| **Javassist** | 源码级 | 直接写 Java 字符串插入代码 | 快速原型、老项目 |
| **ByteKit（Arthas）** | 中间层 | 基于 ASM，Arthas 自用 | JVM 诊断 |

ASM 工作于 **Visitor 模式**：`ClassReader` 解析字节码，`ClassVisitor`/`MethodVisitor` 链式改写，`ClassWriter` 输出新字节码。Byte Buddy 在其上封装了类型安全的 Java API，让"在方法前后插代码"变成几行链式调用。

### 8.5 完整示例：用 Byte Buddy 写统计方法耗时的 Java Agent

**目标**：在不改业务代码的前提下，打印所有标注了 `@Timed` 注解的方法的执行耗时。

**项目结构**：

```
timing-agent/
├── src/main/java/io/example/
│   ├── TimingAgent.java        // premain 入口
│   ├── TimingInterceptor.java  // 耗时统计逻辑
│   └── Timed.java              // 自定义注解
└── pom.xml
```

**Timed.java**：

```java
package io.example;
import java.lang.annotation.*;

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface Timed {}
```

**TimingInterceptor.java**（Byte Buddy 的拦截器）：

```java
package io.example;
import net.bytebuddy.implementation.bind.annotation.*;
import java.lang.reflect.Method;
import java.util.concurrent.Callable;

public class TimingInterceptor {
    @RuntimeType
    public static Object intercept(
            @Origin Method method,
            @SuperCall Callable<?> superCall
    ) throws Exception {
        long start = System.nanoTime();
        try {
            return superCall.call();
        } finally {
            long elapsed = System.nanoTime() - start;
            System.out.printf("[Timed] %s.%s took %.3f ms%n",
                method.getDeclaringClass().getSimpleName(),
                method.getName(),
                elapsed / 1_000_000.0);
        }
    }
}
```

**TimingAgent.java**：

```java
package io.example;
import net.bytebuddy.agent.builder.AgentBuilder;
import net.bytebuddy.implementation.MethodDelegation;
import net.bytebuddy.matcher.ElementMatchers;
import java.lang.instrument.Instrumentation;

public class TimingAgent {
    public static void premain(String args, Instrumentation inst) {
        new AgentBuilder.Default()
            // 匹配任何类中带 @Timed 注解的方法
            .type(ElementMatchers.any())
            .transform((builder, typeDescription, classLoader, module, domain) ->
                builder.method(ElementMatchers.isAnnotatedWith(Timed.class))
                       .intercept(MethodDelegation.to(TimingInterceptor.class))
            )
            .installOn(inst);
        System.out.println("[TimingAgent] installed, watching @Timed methods");
    }
}
```

**pom.xml 关键依赖**：

```xml
<dependency>
    <groupId>net.bytebuddy</groupId>
    <artifactId>byte-buddy</artifactId>
    <version>1.14.18</version>
</dependency>
<dependency>
    <groupId>net.bytebuddy</groupId>
    <artifactId>byte-buddy-agent</artifactId>
    <version>1.14.18</version>
</dependency>
```

**MANIFEST.MF**（由 `maven-jar-plugin` 配置生成）：

```
Manifest-Version: 1.0
Premain-Class: io.example.TimingAgent
Agent-Class: io.example.TimingAgent
Can-Retransform-Classes: true
Can-Redefine-Classes: true
```

Maven 配置：

```xml
<plugin>
    <groupId>org.apache.maven.plugins</groupId>
    <artifactId>maven-jar-plugin</artifactId>
    <configuration>
        <archive>
            <manifestEntries>
                <Premain-Class>io.example.TimingAgent</Premain-Class>
                <Agent-Class>io.example.TimingAgent</Agent-Class>
                <Can-Retransform-Classes>true</Can-Retransform-Classes>
            </manifestEntries>
        </archive>
    </configuration>
</plugin>
```

**打包与运行**：

```bash
mvn package -DskipTests
# 生成 timing-agent-1.0.jar

# 业务代码
java -javaagent:timing-agent-1.0.jar -jar myapp.jar
# 输出：[Timed] OrderService.createOrder took 32.157 ms
```

**动态 attach 方式**（适用于已在运行的进程）：

```java
import com.sun.tools.attach.VirtualMachine;

public class AttachMain {
    public static void main(String[] args) throws Exception {
        String pid = args[0];
        String agentJar = args[1];
        VirtualMachine vm = VirtualMachine.attach(pid);
        vm.loadAgent(agentJar, "");
        vm.detach();
    }
}
```

```bash
# 找到目标进程 PID
jps -l
# 动态注入
java -cp tools.jar AttachMain 12345 timing-agent-1.0.jar
```

### 8.6 工程实战：APM / 诊断工具的视角

理解了上面的机制，APM 框架的原理就清晰了：

**SkyWalking / Pinpoint**：
- 发布为 `-javaagent` 形式，`premain` 中注册若干 `ClassFileTransformer`。
- 每个插件（Dubbo、MySQL JDBC、Spring MVC 等）都是一个独立 transformer，匹配对应类名。
- 在方法入口/出口注入 `Span` 创建/结束代码，通过 `ThreadLocal` / `AsyncContext` 跨异步边界传递。

**Arthas**：
- 用 Attach API 动态注入，无需重启。
- `watch` / `trace` / `redefine` 命令底层就是调用 `inst.retransformClasses()`，每次命令生成一份新字节码（基于 ASM），redefine 后立即生效。

**Mockito inline mock**：
- 普通 Mockito 只能 mock 接口/非 final 类（用 JDK 动态代理/子类代理）。
- Mockito inline 通过 `-javaagent:mockito-agent.jar` 或 `ByteBuddyAgent.install()` 动态 attach，retransform 目标类让它的方法变成可被 mock 的形态，这就是为什么它能 mock `final` 类和 `static` 方法。

### 8.7 陷阱与边界

- **Bootstrap ClassLoader 问题**：`java.*` 核心类由 Bootstrap loader 加载，transform 时 `loader` 参数为 `null`，部分 API（如 Byte Buddy 的 `@SuperCall`）在这些类上不可用，需改用 `Advice` 模式。
- **类循环依赖**：transformer 内部如果 import 了还未加载的类，会触发新的类加载再次进 transformer，造成死锁。Byte Buddy 通过 `AgentBuilder.Default()` 的 `ignored()` 排除系统类解决此问题。
- **retransform 与 redefine 区别**：retransform 会重新走所有已注册 transformer 链（协作式），redefine 是直接覆盖（独裁式），后者会跳过其他 transformer。
- **Java 9+ 模块系统**：需要在启动参数加 `--add-opens` 才能 instrument JDK 内部模块的类（如 `java.net.http`）。

---

## 9. .NET 运行时 Hook

### 9.1 .NET 的两层插装平面

.NET 与 Java 的关键区别：JVM 在 JIT 之前介入（字节码层），.NET 可以在**更靠下**的层次介入——CLR Profiling API 甚至能在 JIT 生成机器码**之后**、方法被调用**之前**替换函数入口指针（类似 x86 的 `jmp` patch），而托管层的 Harmony 库则走这条路。

两层插装平面：

| 层次 | 机制 | 入场方式 | 典型用途 |
|---|---|---|---|
| **非托管层** | CLR Profiling API（`ICorProfilerCallback`）| 环境变量注入 DLL | APM 探针、.NET Diagnostic |
| **托管层** | Harmony / MonoMod | NuGet 包、代码引用 | 游戏 Mod、Unity 插件、单元测试 |

### 9.2 CLR Profiling API（非托管层）

CLR 暴露了一套 COM 接口，允许原生 DLL 以"观察者/改写者"身份嵌入运行时：

**挂载方式**：纯靠环境变量，不修改被监控进程的代码或配置文件：

```bash
# Windows
set CORECLR_ENABLE_PROFILING=1
set CORECLR_PROFILER={YOUR-GUID-HERE}
set CORECLR_PROFILER_PATH=C:\apm\profiler.dll

dotnet run
```

**核心接口**：

- `ICorProfilerCallback`：CLR 在各种事件（类加载、JIT 编译开始/结束、异常、GC、线程创建）时回调你的 DLL。
- `ICorProfilerInfo`：Profiler 主动查询/修改运行时状态的接口，包括：
  - `SetILFunctionBody(moduleId, methodId, newILBytes)`：在方法 JIT 编译前替换 IL（与 Java `ClassFileTransformer` 等价）。
  - `SetILInstrumentedCodeMap()`：保持 IL 与源码的行号映射（用于调试器能正确显示行号）。
  - `RequestReJIT()`：对已 JIT 的方法请求重新编译（类似 Java `retransformClasses`）。

**APM 探针工作流程**（以 Datadog .NET Tracer 为例）：

1. 环境变量指向 `Datadog.Trace.ClrProfiler.Native.dll`。
2. CLR 在每个方法 JIT 编译前调用 `JITCompilationStarted` 回调。
3. Profiler 检查方法签名，若命中目标（如 `System.Net.HttpClient.SendAsync`），调用 `SetILFunctionBody` 注入 Span 创建/结束逻辑。
4. JIT 继续编译已修改的 IL，生成带探针的机器码。

**特点与限制**：
- 能 hook `System.*` 内部方法（Java agent 对 JDK 内部类有模块壁垒）。
- Profiler DLL 本身是非托管代码（C++），与 .NET 代码互操作需要 P/Invoke 或 COM interop。
- 生产可用，性能开销与 Java agent 相当（< 3%）。

### 9.3 System.Reflection.Emit / DynamicMethod（托管动态生成）

.NET 托管层可以在运行时动态生成 IL 并执行，无需写文件：

```csharp
using System;
using System.Reflection;
using System.Reflection.Emit;

// 动态生成一个将两个 int 相加的方法
DynamicMethod addMethod = new DynamicMethod(
    "Add",
    typeof(int),
    new[] { typeof(int), typeof(int) },
    typeof(Program).Module
);

ILGenerator il = addMethod.GetILGenerator();
il.Emit(OpCodes.Ldarg_0);   // 压入参数 0
il.Emit(OpCodes.Ldarg_1);   // 压入参数 1
il.Emit(OpCodes.Add);       // 相加
il.Emit(OpCodes.Ret);       // 返回

var add = (Func<int, int, int>)addMethod.CreateDelegate(typeof(Func<int, int, int>));
Console.WriteLine(add(3, 4));   // 7
```

`DynamicMethod` 在运行时 JIT 编译，避免写临时 `.dll` 文件，是依赖注入容器（Autofac、Castle Windsor）生成构造器委托的常用手段，比反射调用快 10-50 倍。

### 9.4 Harmony 库——托管层方法 Patch

[Harmony](https://github.com/pardeike/Harmony) 是 .NET/Mono 游戏 mod 社区的事实标准，BepInEx（RimWorld、Dyson Sphere Program 等游戏的 mod 框架）内置它。原理：在运行时修改目标方法的**原生函数入口地址**（JIT 编译后的机器码入口），插入一个 `jmp` 跳转到 trampoline，从而实现不改源码的方法拦截。

**三种 patch 方式**：

| 类型 | 执行时机 | 典型用途 |
|---|---|---|
| `Prefix` | 原方法执行前 | 参数检查、短路返回、记录入参 |
| `Postfix` | 原方法执行后 | 修改返回值、记录出参、补充日志 |
| `Transpiler` | 改写 IL 指令流 | 精确修改方法内部逻辑 |

**完整示例：用 Harmony 给任意方法注入耗时日志**

NuGet 依赖：

```xml
<PackageReference Include="Harmony" Version="2.3.3" />
```

目标类（假设是第三方库，不能改源码）：

```csharp
// 假设这是第三方库中的类
public class PaymentService {
    public bool ProcessPayment(string orderId, decimal amount) {
        // ... 复杂支付逻辑 ...
        System.Threading.Thread.Sleep(100);   // 模拟耗时
        return true;
    }
}
```

Patch 类：

```csharp
using HarmonyLib;
using System;
using System.Diagnostics;
using System.Reflection;

// [HarmonyPatch] 标注 + static 方法是 Harmony 的约定
[HarmonyPatch(typeof(PaymentService), nameof(PaymentService.ProcessPayment))]
public static class PaymentServicePatch {

    // Prefix：方法执行前调用
    // __instance 是调用对象的 this，__0/__1 分别对应第一、二个参数
    static void Prefix(string orderId, decimal amount) {
        Console.WriteLine($"[Harmony:Prefix] ProcessPayment called: order={orderId}, amount={amount}");
        // 若 Prefix 返回 false，原方法会被跳过（短路）
    }

    // Postfix：方法执行后调用
    // __result 是原方法的返回值（ref 允许修改它）
    static void Postfix(string orderId, ref bool __result) {
        Console.WriteLine($"[Harmony:Postfix] ProcessPayment returned: {__result}");
        // 可强制改返回值：__result = false;
    }
}
```

主程序中初始化 Harmony：

```csharp
using HarmonyLib;

class Program {
    static void Main(string[] args) {
        // 创建 Harmony 实例，id 用于区分不同 mod 的 patch（方便调试/卸载）
        var harmony = new Harmony("com.example.timing-patch");

        // 自动扫描当前程序集中所有带 [HarmonyPatch] 的类并应用
        harmony.PatchAll(Assembly.GetExecutingAssembly());

        // 运行业务代码
        var svc = new PaymentService();
        svc.ProcessPayment("order-001", 99.9m);
        // 输出：
        // [Harmony:Prefix] ProcessPayment called: order=order-001, amount=99.9
        // [Harmony:Postfix] ProcessPayment returned: True
    }
}
```

**卸载 patch**（测试或临时 hook 结束后）：

```csharp
harmony.UnpatchAll("com.example.timing-patch");
```

### 9.5 Transpiler——IL 级精确改写

当你需要在方法**内部某一行**插入代码（而不是首尾），Transpiler 更合适：

```csharp
using HarmonyLib;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;

[HarmonyPatch(typeof(PaymentService), nameof(PaymentService.ProcessPayment))]
public static class PaymentTranspilerPatch {
    static IEnumerable<CodeInstruction> Transpiler(IEnumerable<CodeInstruction> instructions) {
        foreach (var instr in instructions) {
            // 在每条 ret 指令之前插入一条日志调用
            if (instr.opcode == OpCodes.Ret) {
                yield return new CodeInstruction(OpCodes.Call,
                    typeof(PaymentTranspilerPatch)
                        .GetMethod(nameof(LogReturn), BindingFlags.Static | BindingFlags.NonPublic));
            }
            yield return instr;
        }
    }

    static void LogReturn() {
        Console.WriteLine("[Transpiler] about to return from ProcessPayment");
    }
}
```

Transpiler 操作的是 IL 指令序列，比 Prefix/Postfix 更底层、更强大，但也更容易因为 IL 顺序出错而导致 `InvalidProgramException`——建议配合 dnSpy 调试 IL。

### 9.6 与 Java Agent 的横向对比

| 维度 | Java Agent（Byte Buddy/ASM）| .NET CLR Profiling | .NET Harmony |
|---|---|---|---|
| 操作层次 | 字节码（JIT 前）| IL（JIT 前）或机器码入口（JIT 后）| 机器码入口（JIT 后） |
| 是否需要非托管代码 | 否（纯 Java）| 是（C++ DLL）| 否（纯 C#）|
| 可改范围 | 方法体 IL（不能改签名）| 方法体 IL / 函数入口跳转 | 函数入口跳转（Prefix/Postfix）或 IL（Transpiler） |
| 生产 APM | 是（SkyWalking/Pinpoint）| 是（Datadog/.NET Tracer）| 否（主要用于游戏 Mod/测试）|
| 热更新 | retransformClasses | RequestReJIT | UnpatchAll/Patch |
| JDK/BCL 内部类 | 需要 `--add-opens`（Java 9+）| 可直接 hook `System.*`| 可直接 hook `System.*`（如 `HttpClient`） |

### 9.7 工程实战：游戏 Mod 与 Unity

Harmony 在游戏社区大量使用的原因：

- **Unity/Mono 运行时**：大多数 Unity 游戏用 Mono 或 IL2CPP 后端。Mono 版本支持 Harmony 完整功能；IL2CPP 把 IL 编译成 C++ 再编译，方法签名变化，需要用 `MonoMod.RuntimeDetour` 的 native detour 而不是标准 Harmony patch。
- **BepInEx 框架**：游戏启动时注入 BepInEx loader（用 .NET Core hostfxr hook 或 Doorstop），然后加载各 mod 的 `.dll`，每个 mod 各自创建 `Harmony` 实例、注册 patch，互不干扰（因为 Harmony id 隔离）。
- **协作而非覆盖**：多个 mod 都 patch 同一方法时，Harmony 会**链式**调用所有 Prefix/Postfix（按注册顺序），而不是后来者覆盖先来者——这是比普通 monkey patch 更优雅的多方协作机制。

### 9.8 陷阱与边界

- **AOT 编译（Native AOT / IL2CPP）**：Harmony 依赖 JIT 运行时；AOT 编译的二进制文件没有 JIT，Harmony 无法工作，需要改用 source generator 或 compile-time weaving（如 Fody）。
- **Postfix 无法短路**：Postfix 在原方法已执行后调用，无法阻止原方法的副作用（数据库写入等）；要短路必须用 Prefix 返回 `false`。
- **Prefix 返回 false 的副作用**：Prefix 短路后，原方法的 out/ref 参数不会被初始化，Postfix 中取到的 `__result` 是默认值，需要在 Prefix 里手动设置。
- **线程安全**：Harmony 的 `PatchAll` / `Unpatch` 操作本身不是线程安全的，在多线程初始化时需要加锁。
- **NativeAOT + .NET 8**：Microsoft 推荐用 `System.Diagnostics.DiagnosticSource` + `Activity` 做托管层的分布式 trace，无需底层 hook，适合无 JIT 环境。

---

下一篇：[05-qt-hook.md](./05-qt-hook.md)
