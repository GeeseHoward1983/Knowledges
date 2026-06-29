# 01 · 前端框架 Hooks（React / Vue）

> 本篇覆盖 **React 18/19 Hooks** 与 **Vue 3 Composition API** 的内部机制、所有内置 hook 的工作原理、写自定义 hook 的工程化方法，以及并发渲染下的规则与陷阱。
> 假设读者已经会用 React/Vue 写过组件，但想真正理解"为什么 hook 只能在顶层调用"、"effect 到底什么时候执行"、"setup 和组件实例的关系"等问题。

---

## 1. 概念与适用场景

### 1.1 为什么需要 Hooks

在 Hooks 出现前（React 16.8 以前），有状态逻辑只能写在 **class 组件**里。这带来三大痛点：

1. **逻辑复用难**：跨组件复用有状态逻辑必须靠 HOC 或 render props，造成"嵌套地狱"和 prop 命名冲突。
2. **生命周期割裂**：相关的逻辑（订阅 + 取消订阅）被强行拆到 `componentDidMount` 和 `componentWillUnmount`；不相关的逻辑（订阅、数据获取、动画）混在同一个生命周期函数里。
3. **class 本身的语义负担**：`this`、`bind`、构造器、需要继承——大多数函数式 UI 根本不需要 OOP。

Hooks 的核心目的是：**让函数组件拥有 class 才有的能力（state、生命周期、context），并以"按需组合"的方式取代"按生命周期切割"。**

Vue 3 的 Composition API 解决的是同一类问题（Options API 里 `data`/`methods`/`watch` 把同一功能拆散到不同选项块），思路也几乎相同——把状态和副作用按"逻辑关注点"组织。

### 1.2 适用场景

- **任何函数组件**的状态、副作用、性能优化。
- **跨组件复用有状态逻辑**（自定义 hook 是最佳实践）。
- **集成命令式 API**（DOM 操作、第三方库、Web API）——`useEffect` + `useRef`。
- **派生数据 / 缓存计算**——`useMemo` / `useCallback`。
- **跨层级共享数据**——`useContext`。
- **复杂状态机**——`useReducer`。

---

## 2. 底层原理

### 2.1 React Hook 是如何"记住"状态的？

React 函数组件每次渲染都会重新执行整个函数体。`useState` 怎么记住上一次的值？答案是 **React 在内部为每一个组件实例维护一个 hook 链表**，渲染时按调用顺序逐个对应。

伪代码（约等于 React 内部实现的最小化版本）：

```js
// 全局状态
let currentComponent = null;     // 正在渲染的 fiber
let workInProgressHook = null;   // 链表游标

function useState(initial) {
  const fiber = currentComponent;
  let hook;

  if (fiber.alternate === null) {
    // 首次渲染：创建新 hook 节点
    hook = { state: initial, next: null };
    if (workInProgressHook === null) {
      fiber.memoizedState = hook;          // 链表头
    } else {
      workInProgressHook.next = hook;      // 追加到尾部
    }
    workInProgressHook = hook;
  } else {
    // 更新渲染：从上次链表复用
    hook = workInProgressHook === null
      ? fiber.alternate.memoizedState
      : workInProgressHook.next;
    workInProgressHook = hook;
  }

  const setState = (val) => {
    hook.state = typeof val === 'function' ? val(hook.state) : val;
    scheduleUpdate(fiber);   // 把 fiber 标记为 dirty，进入调度
  };

  return [hook.state, setState];
}
```

**关键点**：

- Hook 节点按调用顺序串成链表，存在 `fiber.memoizedState` 上。Fiber 是 React 16+ 的内部协调（reconciliation）数据结构，每个组件实例对应一个 fiber。
- 渲染时按"游标顺序"取节点。这就是为什么 **hooks 必须每次以相同顺序、相同数量被调用**——一旦顺序变了，链表对应错位，下一个 hook 会拿到错误状态。
- 这条规则被官方称作 **Rules of Hooks**：
  - 只在顶层调用 hook（不能在 `if`/`for`/`return` 之后）。
  - 只在 React 函数（函数组件、自定义 hook）里调用。

ESLint 插件 `eslint-plugin-react-hooks` 就是静态检查这两条规则。

### 2.2 useEffect 的生命周期

`useEffect(create, deps)` 不是同步执行的，它分三步：

1. **渲染阶段**：React 调用组件函数，`useEffect` 只把 `create` 和 `deps` 注册到 fiber 的 `updateQueue`，不执行 effect。
2. **提交阶段（commit phase）**：DOM 被同步更新。React **同步**调用上一次渲染遗留的 `cleanup`（如果有），然后**异步**（在浏览器绘制后）调用新的 `create`。
3. **下次更新或卸载时**：调用本次 `create` 返回的 `cleanup`。

时间线（一次 props 改变触发的重渲染）：

```
T0  React 决定要更新组件
T1  调用组件函数 → 返回新 JSX
T2  执行 reconciliation，确定 DOM diff
T3  ┌─ commit 开始（同步） ─┐
T4  │   执行 useLayoutEffect 的 cleanup（上次）
T5  │   写入 DOM
T6  │   执行 useLayoutEffect 的 create（本次） ← 浏览器还没绘制
T7  └─────────────────────┘
T8  浏览器绘制（paint）
T9  ┌─ 调度异步任务 ─────┐
T10 │   执行 useEffect 的 cleanup（上次）
T11 │   执行 useEffect 的 create（本次）
T12 └────────────────────┘
```

**对比 `useLayoutEffect`**：它在 T4-T6 同步执行（DOM 已更新但浏览器尚未绘制），适合需要在用户看到画面之前测量布局或重新调整 DOM 的场景（避免闪烁）。代价是会阻塞绘制。

### 2.3 依赖数组的本质

`useEffect(fn, [a, b])` 中的 `[a, b]` 是 React 用 **Object.is** 做浅比较的依赖列表。比较结果：

- 上次的 `[a, b]` 和本次的 `[a, b]` **任一不同** → 先跑 cleanup，再跑新 create。
- 全相同 → 跳过 effect。
- `deps` 不传 → 每次渲染都跑（很少需要）。
- `deps` 是 `[]` → 只在 mount 时跑一次，unmount 时 cleanup（注意：在严格模式下开发环境会跑两次！）。

**为什么严格模式跑两次？** 自 React 18，`<StrictMode>` 会刻意 mount → unmount → mount 来检测"组件是否能正确处理被重复挂载"。这是为未来的"可恢复 UI"（如返回后台后继续上次状态）做准备。任何依赖"只跑一次"的副作用，都应该用 cleanup 让它**可重入**。

### 2.4 Vue 3 Composition API 的实现

Vue 3 的实现路线完全不同。`setup()` 函数**每个组件实例只跑一次**（除非组件被销毁重建）。响应式由 **Proxy** 实现：

```js
import { reactive, effect } from '@vue/reactivity';

const state = reactive({ count: 0 });

effect(() => {
  console.log('count =', state.count);  // 这个 effect 自动追踪 state.count
});

state.count++;   // 触发 effect 重跑
```

`reactive()` 用 `new Proxy(target, handler)` 在 get 时记录"当前 effect 依赖了哪个 key"，set 时通知所有相关 effect 重跑。这套机制叫 **响应式依赖追踪**。组件本身就是一个大 effect——`setup()` 返回的 render function 被包成 effect，自动追踪它读到的所有响应式数据。

**对比 React**：
| 维度 | React | Vue 3 |
|---|---|---|
| 组件函数 | 每次更新重跑 | 只跑一次（setup） |
| 状态记忆 | 链表 + 调用顺序 | Proxy + 闭包 |
| 依赖追踪 | 手动 deps 数组 | 自动 |
| 重渲染触发 | setState → 调度 | 响应式写入 → effect 队列 |
| 副作用 API | useEffect / useLayoutEffect | watchEffect / watch / onMounted... |

---

## 3. React 内置 Hooks 全集（速查）

### 3.1 状态类

```jsx
const [state, setState] = useState(initialValue)
const [state, dispatch] = useReducer(reducer, initialArg, init?)
```

`useState` 的进阶用法：

```jsx
// 1. 惰性初始化：initialValue 太昂贵时
const [data, setData] = useState(() => expensiveCompute());

// 2. 函数式更新：基于上一个值
setCount(prev => prev + 1);

// 3. 同 state 两次设成相同值，React 会跳过本次渲染（Object.is）
setCount(5); setCount(5);  // 只触发一次渲染
```

`useReducer` 适合多字段联动、状态机：

```jsx
const initialState = { status: 'idle', data: null, error: null };

function reducer(state, action) {
  switch (action.type) {
    case 'fetch':   return { status: 'loading', data: null, error: null };
    case 'success': return { status: 'done',    data: action.payload, error: null };
    case 'error':   return { status: 'error',   data: null, error: action.payload };
    default: throw new Error('Unknown action');
  }
}

function MyComponent() {
  const [state, dispatch] = useReducer(reducer, initialState);
  // ...
}
```

### 3.2 副作用类

```jsx
useEffect(create, deps?)              // 异步、绘制后
useLayoutEffect(create, deps?)        // 同步、绘制前
useInsertionEffect(create, deps?)     // 在 DOM 变更前、给 CSS-in-JS 库注入样式用，应用代码几乎用不到
```

### 3.3 性能优化类

```jsx
const memoized = useMemo(() => compute(a, b), [a, b]);
const stableFn  = useCallback((x) => doSomething(x, a), [a]);
```

`useCallback(fn, deps)` 等价于 `useMemo(() => fn, deps)`。

**重要**：不是"加了 useMemo 就一定更快"。每次渲染 React 都要做依赖比较和值存取，本身有开销。**只在以下情况下用**：

- 返回值是引用类型，需要传给 `React.memo`/`useEffect`/`useMemo` 的 deps（避免下游误判变化）。
- 计算本身昂贵（>1ms）。

### 3.4 Ref / DOM

```jsx
const ref = useRef(initialValue)      // ref.current 可变，更新不触发渲染
useImperativeHandle(ref, () => ({...}), deps?)  // 自定义 ref 暴露的方法
```

`useRef` 两大用途：

1. **拿 DOM 节点**：`<input ref={ref} />` → `ref.current` 就是 DOM 元素。
2. **跨渲染保存可变值**（计时器 ID、上一次的 props、订阅句柄）。

```jsx
function Timer() {
  const idRef = useRef(null);
  useEffect(() => {
    idRef.current = setInterval(() => console.log('tick'), 1000);
    return () => clearInterval(idRef.current);
  }, []);
  return null;
}
```

### 3.5 Context

```jsx
const value = useContext(MyContext)
```

`useContext` 订阅 Provider 的值；Provider 变化时所有消费者重渲染（无 selector）。复杂场景下用 `useSyncExternalStore` 配合外部 store（如 Zustand）做细粒度订阅。

### 3.6 React 18 新增

```jsx
const [isPending, startTransition] = useTransition()
const deferredValue = useDeferredValue(value)
const id = useId()                       // SSR 安全的稳定 id
useSyncExternalStore(subscribe, getSnapshot, getServerSnapshot?)
```

- **useTransition**：把更新标记为"非紧急"（transition），React 会先渲染紧急更新（如输入框文字），延后渲染 transition 部分。
- **useDeferredValue**：传一个值，返回"延迟版本"——在系统空闲时才更新。等价于把"用值的地方"放进 transition。
- **useId**：为同一组件生成稳定唯一 id，跨 SSR/CSR 一致；不要用它做 list key。
- **useSyncExternalStore**：让外部数据源在并发模式下也能安全订阅（解决 tearing）。

### 3.7 React 19 新增

```jsx
const value = use(promiseOrContext)
const [state, formAction, isPending] = useActionState(action, initialState)
const { pending, data, method, action } = useFormStatus()
const optimistic = useOptimistic(state, updateFn)
```

- **use(promise)**：在渲染过程中"读"一个 promise，未完成会触发 Suspense。是第一个**可以条件调用**（在 if 里）的 hook——它不依赖调用顺序，而是依赖外部 promise 身份。
- **use(context)**：等价于 `useContext`，但也可以条件调用。
- **useActionState / useFormStatus / useOptimistic**：服务器 Actions（RSC）相关，让表单和乐观更新更声明式。

> 注意 React 19 仍在快速演进。具体 API 行为以你使用的小版本官方文档为准（可通过 Context7 拉取最新 `/facebook/react` 文档）。

---

## 4. 完整可运行示例

### 4.1 自定义 Hook：`useLocalStorage`

封装"持久化到 localStorage 的 state"，支持 SSR、跨标签页同步、JSON 序列化。

```jsx
// useLocalStorage.js
import { useCallback, useEffect, useRef, useState, useSyncExternalStore } from 'react';

const isBrowser = typeof window !== 'undefined';

// 跨标签页订阅：用 storage 事件
function subscribe(callback) {
  if (!isBrowser) return () => {};
  window.addEventListener('storage', callback);
  return () => window.removeEventListener('storage', callback);
}

function getSnapshot(key) {
  if (!isBrowser) return null;
  return localStorage.getItem(key);
}

function getServerSnapshot() {
  return null;   // SSR 时永远返回 null，避免 hydration 不匹配
}

export function useLocalStorage(key, initialValue) {
  // useSyncExternalStore 保证并发模式下不 tearing
  const raw = useSyncExternalStore(
    subscribe,
    useCallback(() => getSnapshot(key), [key]),
    getServerSnapshot,
  );

  const value = raw === null
    ? (typeof initialValue === 'function' ? initialValue() : initialValue)
    : JSON.parse(raw);

  const setValue = useCallback((next) => {
    const resolved = typeof next === 'function' ? next(value) : next;
    localStorage.setItem(key, JSON.stringify(resolved));
    // 手动派发一个事件，让同一标签页的其他 useLocalStorage 也更新
    window.dispatchEvent(new StorageEvent('storage', { key }));
  }, [key, value]);

  return [value, setValue];
}
```

使用：

```jsx
function ThemeToggle() {
  const [theme, setTheme] = useLocalStorage('theme', 'light');
  return (
    <button onClick={() => setTheme(t => t === 'light' ? 'dark' : 'light')}>
      Theme: {theme}
    </button>
  );
}
```

要点：

- 用 `useSyncExternalStore` 替代 `useState + useEffect`，避免 React 18 并发模式下两次渲染读到不同值（tearing）。
- SSR 阶段 `getServerSnapshot` 返回 `null`，hydration 后才用真实值。
- 手动派发 `storage` 事件解决"同一标签页多个组件用同一 key"的同步问题（浏览器默认只在跨标签页时派发）。

### 4.2 自定义 Hook：`useDebounce`

```jsx
import { useEffect, useState } from 'react';

export function useDebounce(value, delay = 300) {
  const [debounced, setDebounced] = useState(value);
  useEffect(() => {
    const id = setTimeout(() => setDebounced(value), delay);
    return () => clearTimeout(id);   // cleanup 取消上一次未触发的定时器
  }, [value, delay]);
  return debounced;
}

// 使用
function Search() {
  const [q, setQ] = useState('');
  const debouncedQ = useDebounce(q, 500);

  useEffect(() => {
    if (!debouncedQ) return;
    fetch(`/api/search?q=${encodeURIComponent(debouncedQ)}`);
  }, [debouncedQ]);

  return <input value={q} onChange={e => setQ(e.target.value)} />;
}
```

### 4.3 完整可运行的 React 19 计数器（Vite + React 19）

```bash
npm create vite@latest hooks-demo -- --template react
cd hooks-demo
npm install
npm install react@^19 react-dom@^19
```

```jsx
// src/App.jsx
import { useState, useEffect, useRef, useReducer, useTransition } from 'react';

function App() {
  const [count, setCount] = useState(0);
  const renderCountRef = useRef(0);
  renderCountRef.current += 1;

  const [isPending, startTransition] = useTransition();
  const [list, setList] = useState([]);

  useEffect(() => {
    document.title = `Count: ${count}`;
    return () => { document.title = 'React App'; };
  }, [count]);

  return (
    <>
      <p>Render #: {renderCountRef.current}</p>
      <button onClick={() => setCount(c => c + 1)}>+1</button>
      <button onClick={() => {
        startTransition(() => {
          setList(Array.from({ length: 5000 }, (_, i) => i));
        });
      }}>
        Load 5000 items {isPending ? '(pending...)' : ''}
      </button>
      <ul>{list.map(i => <li key={i}>Item {i}</li>)}</ul>
    </>
  );
}

export default App;
```

`npm run dev` 即可体验：点击 "+1" 是紧急更新，点击 "Load 5000 items" 触发的是 transition，期间 "+1" 依然能立即响应。

### 4.4 Vue 3 Composition API 等价示例

```vue
<script setup>
import { ref, computed, watch, onMounted, onUnmounted } from 'vue';

const count = ref(0);
const doubled = computed(() => count.value * 2);

watch(count, (newVal, oldVal) => {
  console.log(`count: ${oldVal} → ${newVal}`);
});

onMounted(() => console.log('mounted'));
onUnmounted(() => console.log('unmounted'));
</script>

<template>
  <p>{{ count }} × 2 = {{ doubled }}</p>
  <button @click="count++">+1</button>
</template>
```

自定义可复用逻辑（"composable"，等价于 React 自定义 hook）：

```js
// composables/useMouse.js
import { ref, onMounted, onUnmounted } from 'vue';

export function useMouse() {
  const x = ref(0), y = ref(0);
  function update(e) { x.value = e.pageX; y.value = e.pageY; }
  onMounted(() => window.addEventListener('mousemove', update));
  onUnmounted(() => window.removeEventListener('mousemove', update));
  return { x, y };
}
```

使用：

```vue
<script setup>
import { useMouse } from './composables/useMouse';
const { x, y } = useMouse();
</script>
<template><p>{{ x }}, {{ y }}</p></template>
```

---

## 5. 进阶话题

### 5.1 闭包陷阱（Stale Closure）

**问题**：

```jsx
function Counter() {
  const [count, setCount] = useState(0);
  useEffect(() => {
    const id = setInterval(() => {
      console.log(count);  // 永远是 0！
    }, 1000);
    return () => clearInterval(id);
  }, []);  // ← 空依赖
  return <button onClick={() => setCount(c => c + 1)}>+1</button>;
}
```

`useEffect` 只跑一次（`deps=[]`），闭包捕获了 mount 时的 `count=0`，之后 setInterval 永远打印 0。

**修法 1：依赖正确化**——把 `count` 放进 deps，让每次变化都重建定时器（但开销大）。

**修法 2：用 ref 同步最新值**

```jsx
const countRef = useRef(count);
useEffect(() => { countRef.current = count; });
useEffect(() => {
  const id = setInterval(() => console.log(countRef.current), 1000);
  return () => clearInterval(id);
}, []);
```

**修法 3：函数式更新**（如果只关心"基于前值更新"）

```jsx
setCount(prev => prev + 1);   // 不依赖闭包里的 count
```

### 5.2 React 18 严格模式下的双调用

`<React.StrictMode>` 在开发环境下会：

- 调用组件函数两次（检测渲染纯度）。
- mount → unmount → 再 mount（检测副作用可重入性）。

**这是 feature 不是 bug**。所有副作用必须设计为"调一次也对、调两次也对"。比如：

```jsx
// ❌ 错：双 mount 会注册两个监听器，但只 cleanup 一次
useEffect(() => {
  socket.connect();
  return () => socket.disconnect();
}, []);

// 上面的写法在严格模式实际是对的，因为 cleanup 也会跑两次：
//   connect → disconnect → connect ✓
```

但要小心**外部资源计数**：

```jsx
// 假设 sharedResource.acquire() 返回时引用计数 +1
useEffect(() => {
  sharedResource.acquire();
  return () => sharedResource.release();
}, []);
// 严格模式下：acquire → release → acquire
// 最终引用计数 +1，正确。
```

### 5.3 自定义 Hook 的"组合 vs 抽象"边界

**好的**自定义 hook：
- 名字以 `use` 开头（让 ESLint 规则能识别）。
- 只暴露你**真正想暴露**的状态和方法，不要透传所有内部状态。
- 返回值用对象更易扩展（增加字段不破坏调用方解构顺序）。
- 内部 effect 的 cleanup 必须完整。

**避免**：
- 把一切都抽成 hook（拉低可读性）。
- 在 hook 里读 props 后又改 props（违反单向数据流）。
- 一个 hook 既返回 setter 又返回 reducer 又返回 ref——拆开。

### 5.4 并发模式下的 tearing

**Tearing（撕裂）**：在一次渲染过程中，同一外部数据源被读了两次，期间发生了写入，导致渲染结果不一致。

在 React 18 同步模式下不会发生（渲染同步、不可打断）；并发模式下，渲染可被中断、低优先级渲染可被丢弃重做，期间外部数据可能变化。

**对策**：用 `useSyncExternalStore`，它保证整个渲染过程读到的快照一致。

### 5.5 Vue 3 watch vs watchEffect

| | watchEffect | watch |
|---|---|---|
| 依赖收集 | 自动（首次同步执行，记录读到的响应式数据） | 显式（你传 source） |
| 拿不到旧值 | ✗ | ✓ |
| 立即执行 | ✓ | 需 `{ immediate: true }` |
| 适用场景 | 副作用依赖明确、想要自动追踪 | 需要旧值、或想精确控制触发源 |

### 5.6 SSR / RSC（React Server Components）下的差异

- **服务端**：`useEffect` / `useLayoutEffect` 都不执行（没有真实 DOM）。`useState` 用初始值，不会触发更新。
- **RSC**（React Server Components）：服务器组件**完全不能用 hook**（除了 React 19 的 `use`，因为 RSC 不重渲染）。所有有状态逻辑必须在客户端组件（`'use client'`）里。

---

## 6. 调试与卸载

### 6.1 React DevTools

- **Profiler** 面板能看到每次渲染的 hook 顺序、每个 hook 的当前值、什么原因导致重渲染。
- 给组件加 `displayName`，hook 列表里就能看到自定义 hook 的名字。
- 配合浏览器的 `Highlight updates when components render` 选项，能定位"无意义重渲染"。

### 6.2 卸载顺序

组件卸载时，React 会按 hooks 的**反向顺序**调用 cleanup（最后注册的最先 cleanup），这一点与 C++ 析构顺序类似。一般不影响逻辑，但当 hook 之间有依赖（如 hook B 监听了 hook A 创建的 store）时要注意。

### 6.3 useDebugValue

```jsx
function useUser(id) {
  const user = ...;
  useDebugValue(user ? `User ${user.name}` : 'Loading');
  return user;
}
```

在 DevTools 里显示自定义标签，调试自定义 hook 很有用。

---

## 7. 常见陷阱总览

| 陷阱 | 表现 | 原因 | 修法 |
|---|---|---|---|
| 条件调用 hook | `Rendered fewer hooks than expected` | 链表错位 | 把条件移到 hook 内部，hook 始终在顶层调 |
| 依赖数组遗漏 | 用到的旧值不更新 | 闭包未刷新 | eslint-plugin-react-hooks 修正 / ref 同步 |
| 在 effect 里 setState 又不放 deps | 无限循环 | 每次渲染都 set，set 又触发渲染 | 加 deps 或加守卫条件 |
| 把对象/数组当 dep | 每次渲染都重跑 | 引用每次都新 | useMemo 稳定引用 / 拆解关心的字段 |
| useEffect 里直接 await | 函数返回 Promise，cleanup 失效 | useEffect 只接受函数或 undefined | 在 effect 内部定义 async 函数再调用 |
| 误用 useLayoutEffect | 卡 SSR / 卡绘制 | 同步阻塞 | 改 useEffect，除非必须测量布局 |
| StrictMode 下副作用执行两次 | 看起来 bug | 设计如此 | 保证 cleanup 完整、可重入 |

---

## 8. 参考资料

- React 官方文档：https://react.dev/reference/react/hooks
- Rules of Hooks：https://react.dev/warnings/invalid-hook-call-warning
- React Source 解读（hooks 实现）：[react/packages/react-reconciler/src/ReactFiberHooks.js](https://github.com/facebook/react/blob/main/packages/react-reconciler/src/ReactFiberHooks.js)
- Dan Abramov, *A Complete Guide to useEffect*：https://overreacted.io/a-complete-guide-to-useeffect/
- Vue 3 Composition API：https://vuejs.org/guide/extras/composition-api-faq.html
- Vue 3 Reactivity 深入：https://vuejs.org/guide/extras/reactivity-in-depth.html

下一篇：[02-windows-api-hook.md](./02-windows-api-hook.md)
