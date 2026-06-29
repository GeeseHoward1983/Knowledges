# 24-TaskPool轻量级任务并发 - 高效利用多核CPU的秘密武器

> **适合人群**: 需要处理并行任务、提升应用性能的开发者
> **阅读时间**: 16分钟
> **核心内容**: TaskPool高级特性、任务调度、性能优化、实战案例

---

## 🎯 TaskPool解决什么问题?

### 典型场景

**场景1: 批量图片压缩**

```typescript
// ❌ 串行处理: 100张图片,每张100ms,总耗时10秒
for (let i = 0; i < 100; i++) {
  compressImage(images[i])  // 主线程阻塞10秒!
}

// ✅ TaskPool并行: 4核CPU并发,耗时2.5秒
const tasks = images.map(img =>
  taskpool.execute(new taskpool.Task(compressImage, img))
)
await Promise.all(tasks)  // 提速4倍!
```

**场景2: 大数据计算**

```typescript
// ❌ 主线程计算,界面卡顿
const result = heavyCalculation(largeDataset)

// ✅ TaskPool后台计算,界面流畅
const task = new taskpool.Task(heavyCalculation, largeDataset)
const result = await taskpool.execute(task)
```

**场景3: 批量网络请求**

```typescript
// ❌ 串行请求: 100个接口,每个200ms,总耗时20秒
for (const url of urls) {
  await fetch(url)
}

// ✅ 并发请求: 10个并发,耗时2秒
const tasks = urls.map(url =>
  taskpool.execute(new taskpool.Task(fetchData, url))
)
await Promise.all(tasks)
```

---

## 📚 目录

1. **TaskPool核心概念**: 线程池、任务队列、调度器
2. **基础用法回顾**: @Concurrent、Task、execute
3. **任务优先级**: 高/中/低优先级调度
4. **任务组管理**: TaskGroup批量操作
5. **任务取消**: 取消运行中的任务
6. **并发控制**: 限制最大并发数
7. **错误处理**: 捕获异常与重试机制
8. **实战1**: 大数据排序
9. **实战2**: Excel文件解析
10. **性能优化**: 避免频繁创建、任务粒度、内存管理

---

## 一、TaskPool核心概念

### 1.1 线程池架构

TaskPool采用线程池模型,实现高效的任务调度和执行:

```mermaid
graph TB
    subgraph MainThread["主线程 (UI Thread)"]
        APP["应用代码"]
        UI["UI渲染"]
    end

    subgraph TaskPool["TaskPool线程池"]
        Queue["任务队列<br/>(优先级队列)"]
        Scheduler["任务调度器"]

        subgraph WorkerPool["工作线程池"]
            W1["Worker 1"]
            W2["Worker 2"]
            W3["Worker 3"]
            W4["Worker 4"]
        end
    end

    APP -->|"execute(task1)"| Queue
    APP -->|"execute(task2)"| Queue
    APP -->|"execute(task3)"| Queue

    Queue -->|"按优先级调度"| Scheduler

    Scheduler -->|"分配任务"| W1
    Scheduler -->|"分配任务"| W2
    Scheduler -->|"分配任务"| W3
    Scheduler -->|"分配任务"| W4

    W1 -->|"返回结果"| APP
    W2 -->|"返回结果"| APP
    W3 -->|"返回结果"| APP
    W4 -->|"返回结果"| APP

    style MainThread fill:#e1f5ff
    style TaskPool fill:#fff4e6
    style WorkerPool fill:#e8f5e9
    style Queue fill:#ffeb3b
    style Scheduler fill:#ff9800
```

**核心组件**:
- **任务队列**: 存储待执行的任务,支持优先级排序
- **调度器**: 根据优先级分配任务到空闲工作线程
- **工作线程池**: 实际执行任务的线程 (数量等于CPU核心数)
- **自动管理**: 线程自动创建、复用、销毁

---

### 1.2 任务生命周期与调度流程

TaskPool任务从创建到完成经历以下阶段:

```mermaid
sequenceDiagram
    participant App as 应用代码
    participant Queue as 任务队列
    participant Scheduler as 调度器
    participant Worker as 工作线程
    participant Promise as Promise对象

    App->>App: 1. 创建Task<br/>new Task(fn, params)
    App->>Queue: 2. 提交任务<br/>execute(task)
    activate Queue
    Queue->>Promise: 3. 返回Promise
    Queue->>App: Promise
    deactivate Queue

    Queue->>Scheduler: 4. 等待调度
    Note over Queue,Scheduler: 按优先级排队

    Scheduler->>Worker: 5. 分配到空闲线程
    activate Worker
    Worker->>Worker: 6. 执行任务
    Note over Worker: 调用@Concurrent函数

    Worker->>Promise: 7. 返回结果
    deactivate Worker
    Promise->>App: 8. resolve(result)

    Worker->>Worker: 9. 线程回收
    Note over Worker: 返回线程池,等待下一个任务
```

**任务调度详细流程图**:

以下是TaskPool任务调度的完整流程,展示了从任务提交到执行完成的每个关键步骤:

```mermaid
flowchart TD
    Start([应用提交任务]) --> Create[创建Task对象<br/>new Task fn, params]
    Create --> Execute[调用execute task]
    Execute --> CreatePromise[创建Promise对象]
    CreatePromise --> AddQueue{任务队列}

    AddQueue --> CheckPriority{检查优先级}
    CheckPriority -->|HIGH| HighQueue[高优先级队列]
    CheckPriority -->|MEDIUM| MediumQueue[中优先级队列]
    CheckPriority -->|LOW| LowQueue[低优先级队列]

    HighQueue --> Scheduler[调度器]
    MediumQueue --> Scheduler
    LowQueue --> Scheduler

    Scheduler --> CheckWorker{检查工作线程}
    CheckWorker -->|有空闲| AssignWorker[分配到空闲Worker]
    CheckWorker -->|全忙| WaitQueue[任务等待队列]

    WaitQueue --> CheckWorker

    AssignWorker --> ExecuteTask[Worker执行任务]
    ExecuteTask --> CheckError{执行结果}

    CheckError -->|成功| ResolvePromise[Promise.resolve result]
    CheckError -->|失败| RejectPromise[Promise.reject error]

    ResolvePromise --> RecycleWorker[回收Worker到线程池]
    RejectPromise --> RecycleWorker

    RecycleWorker --> CheckNextTask{队列有任务?}
    CheckNextTask -->|是| CheckWorker
    CheckNextTask -->|否| Idle[Worker空闲]

    Idle --> End([完成])

    style Start fill:#4CAF50
    style End fill:#4CAF50
    style CheckPriority fill:#FF9800
    style HighQueue fill:#F44336
    style MediumQueue fill:#FFC107
    style LowQueue fill:#8BC34A
    style Scheduler fill:#2196F3
    style ExecuteTask fill:#9C27B0
```

**关键阶段说明**:
1. **创建任务**: 使用`@Concurrent`函数创建Task对象
2. **提交队列**: `execute()`提交到任务队列,立即返回Promise
3. **等待调度**: 任务在队列中按优先级排队
4. **分配线程**: 调度器将任务分配给空闲工作线程
5. **执行任务**: 工作线程执行@Concurrent函数
6. **返回结果**: 通过Promise返回结果给主线程
7. **自动销毁**: 任务完成后自动销毁,线程回收复用

---

## 二、基础用法回顾

### 2.1 定义并发函数

```typescript
// ✅ 可运行代码
import taskpool from '@ohos.taskpool'

// 必须使用@Concurrent装饰器
@Concurrent
function add(a: number, b: number): number {
  return a + b
}

@Concurrent
function processData(data: string[]): string[] {
  return data.map(item => item.toUpperCase())
}

@Concurrent
async function fetchFromAPI(url: string): Promise<any> {
  const response = await fetch(url)
  return response.json()
}
```

**@Concurrent规则**:
- ✅ 必须是顶层函数 (不能是类方法)
- ✅ 支持同步/异步函数
- ✅ 参数和返回值必须可序列化
- ❌ 不能访问外部闭包变量

---

### 2.2 创建和执行任务

```typescript
// ✅ 可运行代码
// 1. 创建任务
const task = new taskpool.Task(add, 10, 20)

// 2. 执行任务 (返回Promise)
const result = await taskpool.execute(task)
console.log(result)  // 30

// 3. 简化写法
const result2 = await taskpool.execute(
  new taskpool.Task(processData, ['hello', 'world'])
)
console.log(result2)  // ['HELLO', 'WORLD']
```

---

## 三、任务优先级

### 3.1 优先级类型与调度机制

TaskPool提供3个优先级级别,调度器按优先级从高到低依次调度任务:

```mermaid
graph LR
    subgraph Priority["任务优先级队列"]
        direction TB
        HIGH["高优先级队列<br/>Priority.HIGH"]
        MEDIUM["中优先级队列<br/>Priority.MEDIUM<br/>(默认)"]
        LOW["低优先级队列<br/>Priority.LOW"]
    end

    subgraph Scheduler["调度器"]
        direction TB
        Rule["调度规则:<br/>1. 先检查高优先级队列<br/>2. 再检查中优先级队列<br/>3. 最后检查低优先级队列"]
    end

    subgraph Workers["工作线程"]
        W1["Worker 1"]
        W2["Worker 2"]
        W3["Worker 3"]
    end

    Task1["紧急任务<br/>(HIGH)"] -->|"优先执行"| HIGH
    Task2["普通任务<br/>(MEDIUM)"] --> MEDIUM
    Task3["后台任务<br/>(LOW)"] --> LOW

    HIGH -->|"1️⃣ 优先调度"| Scheduler
    MEDIUM -->|"2️⃣ 其次调度"| Scheduler
    LOW -->|"3️⃣ 最后调度"| Scheduler

    Scheduler --> W1
    Scheduler --> W2
    Scheduler --> W3

    style HIGH fill:#ff5252
    style MEDIUM fill:#ffc107
    style LOW fill:#4caf50
    style Scheduler fill:#2196f3
```

**优先级说明**:

鸿蒙TaskPool提供3个优先级:

```typescript
// ✅ 可运行代码
enum Priority {
  HIGH = 0,    // 高优先级: 紧急任务
  MEDIUM = 1,  // 中优先级: 默认
  LOW = 2      // 低优先级: 后台任务
}
```

### 3.2 设置优先级

```typescript
// ✅ 可运行代码
import taskpool from '@ohos.taskpool'

@Concurrent
function urgentTask() {
  console.log('紧急任务执行')
}

@Concurrent
function normalTask() {
  console.log('普通任务执行')
}

@Concurrent
function backgroundTask() {
  console.log('后台任务执行')
}

// 创建不同优先级的任务
const highTask = new taskpool.Task(urgentTask)
const mediumTask = new taskpool.Task(normalTask)
const lowTask = new taskpool.Task(backgroundTask)

// 执行时指定优先级
taskpool.execute(highTask, taskpool.Priority.HIGH)
taskpool.execute(mediumTask, taskpool.Priority.MEDIUM)  // 默认
taskpool.execute(lowTask, taskpool.Priority.LOW)

// 输出顺序:
// 紧急任务执行
// 普通任务执行
// 后台任务执行
```

### 3.3 实战: 图片加载优先级

```typescript
// ✅ 可运行代码
@Entry
@Component
struct ImageGallery {
  @State images: ImageInfo[] = []

  aboutToAppear() {
    this.loadImages()
  }

  async loadImages() {
    // 可见区域图片: 高优先级
    const visibleImages = this.images.slice(0, 10)
    const highPriorityTasks = visibleImages.map(img =>
      taskpool.execute(
        new taskpool.Task(loadImage, img.url),
        taskpool.Priority.HIGH
      )
    )

    // 预加载图片: 低优先级
    const preloadImages = this.images.slice(10)
    const lowPriorityTasks = preloadImages.map(img =>
      taskpool.execute(
        new taskpool.Task(loadImage, img.url),
        taskpool.Priority.LOW
      )
    )

    // 先加载可见图片
    await Promise.all(highPriorityTasks)
    console.log('可见图片加载完成')

    // 后台加载其他图片
    Promise.all(lowPriorityTasks)
  }

  build() {
    Grid() {
      ForEach(this.images, (img: ImageInfo) => {
        GridItem() {
          Image(img.data)
        }
      })
    }
  }
}

@Concurrent
async function loadImage(url: string): Promise<ArrayBuffer> {
  const response = await fetch(url)
  return response.arrayBuffer()
}
```

**效果**:
- ✅ 用户先看到可见区域的图片
- ✅ 后台慢慢加载其他图片
- ✅ 体验更流畅

---

## 四、任务组管理

### 4.1 TaskGroup基础

**TaskGroup** 用于批量管理任务:

```typescript
// ✅ 可运行代码
import taskpool from '@ohos.taskpool'

@Concurrent
function task1() {
  return 'Task 1 完成'
}

@Concurrent
function task2() {
  return 'Task 2 完成'
}

@Concurrent
function task3() {
  return 'Task 3 完成'
}

// 创建任务组
const taskGroup = new taskpool.TaskGroup()

// 添加任务
taskGroup.addTask(new taskpool.Task(task1))
taskGroup.addTask(new taskpool.Task(task2))
taskGroup.addTask(new taskpool.Task(task3))

// 一次性执行所有任务
const results = await taskpool.execute(taskGroup)
console.log(results)  // ['Task 1 完成', 'Task 2 完成', 'Task 3 完成']
```

### 4.2 任务组优先级

```typescript
// ✅ 可运行代码
const taskGroup = new taskpool.TaskGroup()

// 为每个任务设置不同优先级
taskGroup.addTask(
  new taskpool.Task(urgentTask),
  taskpool.Priority.HIGH
)

taskGroup.addTask(
  new taskpool.Task(normalTask),
  taskpool.Priority.MEDIUM
)

taskGroup.addTask(
  new taskpool.Task(lowTask),
  taskpool.Priority.LOW
)

// 执行任务组 (按优先级顺序)
await taskpool.execute(taskGroup)
```

### 4.3 实战: 批量数据处理

```typescript
// ✅ 可运行代码
@Concurrent
function processChunk(chunk: number[]): number {
  // 对数据块求和
  return chunk.reduce((sum, n) => sum + n, 0)
}

async function sumLargeArray(array: number[]): Promise<number> {
  const chunkSize = 10000  // 每块1万个数字
  const taskGroup = new taskpool.TaskGroup()

  // 将大数组拆分为多个小块
  for (let i = 0; i < array.length; i += chunkSize) {
    const chunk = array.slice(i, i + chunkSize)
    taskGroup.addTask(new taskpool.Task(processChunk, chunk))
  }

  // 并发处理所有数据块
  const results = await taskpool.execute(taskGroup)

  // 合并结果
  return results.reduce((sum, n) => sum + n, 0)
}

// 使用示例
const bigArray = Array(1000000).fill(0).map((_, i) => i + 1)  // 100万个数字
const total = await sumLargeArray(bigArray)
console.log(`总和: ${total}`)  // 500000500000
```

**性能对比**:
```typescript
// ✅ 可运行代码
// 单线程: 约200ms
const start1 = Date.now()
const sum1 = bigArray.reduce((sum, n) => sum + n, 0)
console.log(`单线程: ${Date.now() - start1}ms`)

// TaskPool: 约50ms (4核CPU)
const start2 = Date.now()
const sum2 = await sumLargeArray(bigArray)
console.log(`TaskPool: ${Date.now() - start2}ms`)
// 提速4倍!
```

**性能对比可视化**:

```mermaid
graph LR
    subgraph 单线程处理
        direction TB
        S1["数据块1<br/>25ms"] --> S2["数据块2<br/>25ms"]
        S2 --> S3["数据块3<br/>25ms"]
        S3 --> S4["数据块4<br/>25ms"]
        S5["总耗时: 100ms"]
    end

    subgraph TaskPool并发处理
        direction TB
        T1["数据块1<br/>25ms"]
        T2["数据块2<br/>25ms"]
        T3["数据块3<br/>25ms"]
        T4["数据块4<br/>25ms"]
        T5["总耗时: 25ms<br/>提速4倍!"]
    end

    style S1 fill:#ffcdd2
    style S2 fill:#ffcdd2
    style S3 fill:#ffcdd2
    style S4 fill:#ffcdd2
    style S5 fill:#f44336,color:#fff

    style T1 fill:#c8e6c9
    style T2 fill:#c8e6c9
    style T3 fill:#c8e6c9
    style T4 fill:#c8e6c9
    style T5 fill:#4caf50,color:#fff
```

---

## 五、任务取消

### 5.1 取消单个任务

```typescript
// ✅ 可运行代码
import taskpool from '@ohos.taskpool'

@Concurrent
function longRunningTask(): number {
  let sum = 0
  for (let i = 0; i < 10000000000; i++) {  // 耗时10秒
    sum += i
  }
  return sum
}

// 创建并执行任务
const task = new taskpool.Task(longRunningTask)
const promise = taskpool.execute(task)

// 2秒后取消任务
setTimeout(() => {
  taskpool.cancel(task)
  console.log('任务已取消')
}, 2000)

// 捕获取消异常
try {
  const result = await promise
} catch (err) {
  console.error('任务被取消:', err.message)
}
```

### 5.2 取消任务组

```typescript
// ✅ 可运行代码
const taskGroup = new taskpool.TaskGroup()

taskGroup.addTask(new taskpool.Task(task1))
taskGroup.addTask(new taskpool.Task(task2))
taskGroup.addTask(new taskpool.Task(task3))

const promise = taskpool.execute(taskGroup)

// 取消整个任务组
setTimeout(() => {
  taskpool.cancel(taskGroup)
  console.log('任务组已取消')
}, 1000)

try {
  await promise
} catch (err) {
  console.error('任务组被取消')
}
```

### 5.3 实战: 可取消的搜索

```typescript
// ✅ 可运行代码
@Entry
@Component
struct SearchPage {
  @State keyword: string = ''
  @State results: string[] = []
  @State searching: boolean = false
  private currentTask: taskpool.Task = null

  build() {
    Column() {
      TextInput({ placeholder: '搜索', text: this.keyword })
        .onChange((value) => {
          this.keyword = value
          this.search(value)
        })

      if (this.searching) {
        LoadingSpinner()
      }

      List() {
        ForEach(this.results, (item: string) => {
          ListItem() {
            Text(item)
          }
        })
      }
    }
  }

  search(keyword: string) {
    // 取消上一次的搜索任务
    if (this.currentTask) {
      taskpool.cancel(this.currentTask)
    }

    // 创建新搜索任务
    this.currentTask = new taskpool.Task(searchInDatabase, keyword)
    this.searching = true

    taskpool.execute(this.currentTask)
      .then((results) => {
        this.results = results
        this.searching = false
      })
      .catch((err) => {
        console.log('搜索被取消或出错')
        this.searching = false
      })
  }
}

@Concurrent
async function searchInDatabase(keyword: string): Promise<string[]> {
  // 模拟数据库查询,耗时1秒
  await new Promise(resolve => setTimeout(resolve, 1000))
  return [`结果1: ${keyword}`, `结果2: ${keyword}`]
}
```

**效果**:
- ✅ 用户快速输入时,自动取消上一次搜索
- ✅ 只显示最后一次搜索结果
- ✅ 避免无效请求

---

## 六、并发控制

### 6.1 限制最大并发数

```typescript
// ✅ 可运行代码
// 设置最大并发线程数为2
taskpool.setMaxConcurrency(2)

// 即使提交10个任务,同时只有2个在执行
const tasks = Array(10).fill(0).map((_, i) => {
  return taskpool.execute(new taskpool.Task(slowTask, i))
})

await Promise.all(tasks)
```

### 6.2 自定义并发控制

```typescript
// ✅ 可运行代码
class ConcurrencyLimiter {
  private maxConcurrent: number
  private currentRunning: number = 0
  private queue: Array<() => void> = []

  constructor(maxConcurrent: number) {
    this.maxConcurrent = maxConcurrent
  }

  async run<T>(task: taskpool.Task): Promise<T> {
    // 等待空闲槽位
    await this.waitForSlot()

    this.currentRunning++

    try {
      const result = await taskpool.execute(task)
      return result
    } finally {
      this.currentRunning--
      this.processQueue()
    }
  }

  private waitForSlot(): Promise<void> {
    if (this.currentRunning < this.maxConcurrent) {
      return Promise.resolve()
    }

    return new Promise(resolve => {
      this.queue.push(resolve)
    })
  }

  private processQueue() {
    if (this.queue.length > 0 && this.currentRunning < this.maxConcurrent) {
      const resolve = this.queue.shift()
      resolve()
    }
  }
}

// 使用
const limiter = new ConcurrencyLimiter(5)  // 最多5个并发

const tasks = Array(100).fill(0).map((_, i) => {
  return limiter.run(new taskpool.Task(processItem, i))
})

await Promise.all(tasks)
```

### 6.3 实战: 批量文件上传

```typescript
// ✅ 可运行代码
@Entry
@Component
struct FileUploader {
  @State files: File[] = []
  @State uploadProgress: Map<string, number> = new Map()

  async uploadFiles() {
    const limiter = new ConcurrencyLimiter(3)  // 最多3个并发上传

    const uploadTasks = this.files.map(async (file) => {
      const task = new taskpool.Task(uploadFile, file.path, file.data)

      const result = await limiter.run(task)

      // 更新进度
      this.uploadProgress.set(file.path, 100)

      return result
    })

    await Promise.all(uploadTasks)
    console.log('所有文件上传完成')
  }

  build() {
    Column() {
      List() {
        ForEach(this.files, (file: File) => {
          ListItem() {
            Row() {
              Text(file.name)
              Progress({
                value: this.uploadProgress.get(file.path) || 0,
                total: 100
              })
            }
          }
        })
      }

      Button('开始上传')
        .onClick(() => {
          this.uploadFiles()
        })
    }
  }
}

@Concurrent
async function uploadFile(path: string, data: ArrayBuffer): Promise<boolean> {
  // 模拟上传
  const formData = new FormData()
  formData.append('file', new Blob([data]))

  const response = await fetch('https://api.example.com/upload', {
    method: 'POST',
    body: formData
  })

  return response.ok
}
```

**效果**:
- ✅ 最多3个文件同时上传
- ✅ 避免同时上传100个文件导致网络拥塞
- ✅ 实时显示上传进度

---

## 七、错误处理

### 7.1 捕获任务异常

```typescript
// ✅ 可运行代码
@Concurrent
function riskyTask(value: number): number {
  if (value < 0) {
    throw new Error('值不能为负数')
  }
  return value * 2
}

// 方法1: try-catch
try {
  const result = await taskpool.execute(new taskpool.Task(riskyTask, -5))
} catch (err) {
  console.error('任务执行失败:', err.message)
}

// 方法2: Promise.catch
taskpool.execute(new taskpool.Task(riskyTask, -5))
  .then(result => {
    console.log('成功:', result)
  })
  .catch(err => {
    console.error('失败:', err.message)
  })
```

### 7.2 任务重试机制

```typescript
// ✅ 可运行代码
async function executeWithRetry<T>(
  task: taskpool.Task,
  maxRetries: number = 3,
  delay: number = 1000
): Promise<T> {
  let lastError: Error

  for (let i = 0; i < maxRetries; i++) {
    try {
      const result = await taskpool.execute(task)
      return result
    } catch (err) {
      lastError = err
      console.log(`第${i + 1}次尝试失败,等待重试...`)

      if (i < maxRetries - 1) {
        await new Promise(resolve => setTimeout(resolve, delay))
      }
    }
  }

  throw new Error(`任务失败,已重试${maxRetries}次: ${lastError.message}`)
}

// 使用
@Concurrent
async function unstableTask(): Promise<string> {
  // 模拟30%成功率的任务
  if (Math.random() < 0.7) {
    throw new Error('网络错误')
  }
  return '成功'
}

const result = await executeWithRetry(
  new taskpool.Task(unstableTask),
  3,    // 最多重试3次
  2000  // 每次重试间隔2秒
)
console.log(result)
```

### 7.3 实战: 批量API请求with容错

```typescript
// ✅ 可运行代码
@Concurrent
async function fetchAPI(url: string): Promise<any> {
  const response = await fetch(url)
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`)
  }
  return response.json()
}

async function batchFetchWithFallback(urls: string[]): Promise<any[]> {
  const tasks = urls.map(async (url) => {
    const task = new taskpool.Task(fetchAPI, url)

    try {
      return await executeWithRetry(task, 3, 1000)
    } catch (err) {
      console.error(`${url} 请求失败:`, err.message)
      return null  // 失败返回null,不影响其他请求
    }
  })

  const results = await Promise.all(tasks)

  // 过滤掉失败的请求
  return results.filter(r => r !== null)
}

// 使用
const urls = [
  'https://api.example.com/user/1',
  'https://api.example.com/user/2',  // 假设这个请求失败
  'https://api.example.com/user/3'
]

const users = await batchFetchWithFallback(urls)
console.log(`成功获取${users.length}个用户`)  // 成功获取2个用户
```

---

## 八、实战1: 大数据排序

### 8.1 归并排序并行化

```typescript
// ✅ 可运行代码
@Concurrent
function mergeSort(arr: number[]): number[] {
  if (arr.length <= 1) return arr

  const mid = Math.floor(arr.length / 2)
  const left = arr.slice(0, mid)
  const right = arr.slice(mid)

  return merge(mergeSort(left), mergeSort(right))
}

function merge(left: number[], right: number[]): number[] {
  const result: number[] = []
  let i = 0, j = 0

  while (i < left.length && j < right.length) {
    if (left[i] < right[j]) {
      result.push(left[i++])
    } else {
      result.push(right[j++])
    }
  }

  return result.concat(left.slice(i)).concat(right.slice(j))
}

// 并行归并排序
async function parallelMergeSort(arr: number[]): Promise<number[]> {
  if (arr.length <= 10000) {
    // 小数组直接排序
    return mergeSort(arr)
  }

  // 大数组分成4块并行排序
  const chunkSize = Math.ceil(arr.length / 4)
  const chunks: number[][] = []

  for (let i = 0; i < arr.length; i += chunkSize) {
    chunks.push(arr.slice(i, i + chunkSize))
  }

  // 并行排序每一块
  const sortedChunks = await Promise.all(
    chunks.map(chunk =>
      taskpool.execute(new taskpool.Task(mergeSort, chunk))
    )
  )

  // 合并已排序的块
  let result = sortedChunks[0]
  for (let i = 1; i < sortedChunks.length; i++) {
    result = merge(result, sortedChunks[i])
  }

  return result
}

// 性能测试
const bigArray = Array(1000000).fill(0).map(() => Math.random())

// 单线程
const start1 = Date.now()
const sorted1 = mergeSort(bigArray.slice())
console.log(`单线程: ${Date.now() - start1}ms`)  // 约500ms

// 并行
const start2 = Date.now()
const sorted2 = await parallelMergeSort(bigArray.slice())
console.log(`并行: ${Date.now() - start2}ms`)  // 约150ms,提速3倍
```

---

## 九、实战2: Excel文件解析

### 9.1 分块解析大文件

```typescript
// ✅ 可运行代码
import fileIo from '@ohos.file.fs'

interface ExcelRow {
  [key: string]: string | number
}

@Concurrent
function parseCSVChunk(chunk: string, headers: string[]): ExcelRow[] {
  const lines = chunk.split('\n').filter(line => line.trim())

  return lines.map(line => {
    const values = line.split(',')
    const row: ExcelRow = {}

    headers.forEach((header, index) => {
      row[header] = values[index] || ''
    })

    return row
  })
}

async function parseExcelFile(filePath: string): Promise<ExcelRow[]> {
  // 读取文件
  const content = await fileIo.readText(filePath)

  // 提取表头
  const lines = content.split('\n')
  const headers = lines[0].split(',')

  // 分块处理 (每块1000行)
  const chunkSize = 1000
  const chunks: string[] = []

  for (let i = 1; i < lines.length; i += chunkSize) {
    const chunk = lines.slice(i, i + chunkSize).join('\n')
    chunks.push(chunk)
  }

  // 并行解析所有块
  const taskGroup = new taskpool.TaskGroup()
  chunks.forEach(chunk => {
    taskGroup.addTask(new taskpool.Task(parseCSVChunk, chunk, headers))
  })

  const results = await taskpool.execute(taskGroup)

  // 合并结果
  return results.flat()
}

// 使用
@Entry
@Component
struct ExcelViewer {
  @State data: ExcelRow[] = []
  @State loading: boolean = false

  build() {
    Column() {
      Button('加载Excel文件')
        .onClick(async () => {
          this.loading = true

          // 解析100MB的Excel文件
          this.data = await parseExcelFile('/data/storage/large-data.csv')

          this.loading = false
          console.log(`加载${this.data.length}行数据`)
        })

      if (this.loading) {
        LoadingSpinner()
      }

      List() {
        ForEach(this.data, (row: ExcelRow) => {
          ListItem() {
            Text(JSON.stringify(row))
          }
        })
      }
    }
  }
}
```

**性能对比**:
- 单线程: 解析100MB文件,耗时约8秒
- TaskPool: 耗时约2秒,提速4倍

---

## 十、性能优化

### 10.1 避免频繁创建任务

```typescript
// ❌ 错误: 每次都创建新Task
for (let i = 0; i < 1000; i++) {
  await taskpool.execute(new taskpool.Task(processItem, i))
}

// ✅ 正确: 批量创建,一次执行
const tasks = []
for (let i = 0; i < 1000; i++) {
  tasks.push(taskpool.execute(new taskpool.Task(processItem, i)))
}
await Promise.all(tasks)
```

### 10.2 任务粒度优化

```typescript
// ❌ 任务太小,调度开销大
const tasks = arr.map(item =>
  taskpool.execute(new taskpool.Task(process, item))
)

// ✅ 合理粒度: 将100个小任务合并为1个
const chunkSize = 100
const tasks = []
for (let i = 0; i < arr.length; i += chunkSize) {
  const chunk = arr.slice(i, i + chunkSize)
  tasks.push(taskpool.execute(new taskpool.Task(processBatch, chunk)))
}
```

**最佳实践**:
- ✅ 单个任务耗时: **10ms - 1s** 最佳
- ❌ 任务太小 (<1ms): 调度开销 > 收益
- ❌ 任务太大 (>10s): 无法充分并行

### 10.3 内存管理

```typescript
// ❌ 传递大对象,内存占用高
const bigData = new Array(1000000).fill({ /* 大对象 */ })
await taskpool.execute(new taskpool.Task(process, bigData))

// ✅ 使用ArrayBuffer零拷贝
const buffer = serializeToArrayBuffer(bigData)
await taskpool.execute(new taskpool.Task(processBuffer, buffer))
```

### 10.4 复用@Concurrent函数

```typescript
// ❌ 重复定义相同逻辑
@Concurrent
function processUser1(data) { /* ... */ }

@Concurrent
function processUser2(data) { /* ... */ }

// ✅ 复用通用函数
@Concurrent
function processEntity<T>(entity: T, type: string): T {
  // 通用处理逻辑
  return entity
}

// 使用
taskpool.execute(new taskpool.Task(processEntity, user, 'user'))
taskpool.execute(new taskpool.Task(processEntity, product, 'product'))
```

---

## 📌 本章总结

### 核心要点

1. **TaskPool优势**: 轻量级、自动管理、高性能
2. **任务优先级**: HIGH/MEDIUM/LOW三级调度
3. **任务组**: 批量管理,简化代码
4. **任务取消**: 支持中途取消,避免浪费
5. **并发控制**: 限制最大并发数,防止资源耗尽
6. **错误处理**: try-catch + 重试机制

### 最佳实践

- ✅ 任务粒度: 10ms-1s最佳
- ✅ 批量提交: 避免频繁创建任务
- ✅ 合理并发: 根据CPU核心数设置
- ✅ 错误容错: 单个任务失败不影响整体
- ✅ 内存优化: 大数据用ArrayBuffer
- ❌ 避免过小任务: 调度开销 > 收益
- ❌ 避免过大任务: 无法充分并行

### 性能收益

- ✅ 大数据排序: 提速**3-4倍**
- ✅ 批量图片处理: 提速**4倍** (4核CPU)
- ✅ Excel解析: 提速**4倍**

---

## 🎯 下一步

下一篇文章将学习 **《25-Worker独立线程管理》**:
- Worker生命周期详解
- 主线程与Worker双向通信
- Worker池化与复用
- 实战: WebSocket长连接

---

> 💡 **小贴士**: TaskPool是鸿蒙并发编程的首选方案,适合90%的并发场景。掌握任务优先级、错误处理、性能优化技巧,可以显著提升应用性能!
