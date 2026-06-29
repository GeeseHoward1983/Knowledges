---
title: CMake Presets
tags: [cmake, presets, workflow, reference]
chapter: "16"
cmake_version: 4.3.4
---

# 第 16 章 · CMake Presets

> 基准版本：CMake 4.3.4

把一长串 `-D` 开关、`-G` 生成器、`--toolchain` 路径、环境变量记在脑子里或散落在各自的 shell 脚本、CI 配置、IDE 设置里，是大型工程长期的痛点：同一个项目，开发者本机这样配、CI 那样配、IDE 又自己一套，配置漂移（configuration drift）几乎不可避免。**CMake Presets** 用一份提交进版本库的 JSON 文件，把"如何配置、如何构建、如何测试、如何打包、如何串成工作流"全部固化下来，让命令行（`cmake --preset`）、IDE（Visual Studio、VS Code、CLion、Qt Creator 都原生支持）、CI 共享**同一个配置入口**。

本章以 CMake **4.3.4** 为基准逐字段核对官方 `cmake-presets(7)` 手册。需要特别强调的是 presets 文件有自己独立的 **schema version（文件格式版本）**，与 `cmake_minimum_required` 里的 CMake 版本是两套数字——本章会在每个字段后标注它由哪个 schema version 引入，这是写出可移植 presets 文件的关键。

---

## 16.1 Presets 是什么 / 为何用

### 16.1.1 从一串 `-D` 说起

不用 presets 时，一次典型的配置往往是这样：

```bash
cmake -S . -B build/linux-debug \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=/opt/tc/aarch64.cmake \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DBUILD_TESTING=ON \
  -DMYPROJ_WITH_FOO=ON
```

这串命令的问题在于：它**只活在某个人的终端历史里**。换台机器、换个同事、换到 CI、换到 IDE 的图形界面，都要重新拼一遍，而且极易拼错或遗漏某个 `-D`。

用 presets 后，上面这一切被写进 `CMakePresets.json` 的一个名为 `linux-debug` 的 preset，调用塌缩成一行：

```bash
cmake --preset linux-debug
```

### 16.1.2 Presets 解决的三个问题

- **统一配置入口**：CLI、IDE、CI 读同一份 JSON。Visual Studio 2019+、VS Code（CMake Tools 插件）、CLion、Qt Creator 都能直接识别并在界面里列出可选 preset，开发者点一下即可，无需记命令。
- **消除配置漂移**：`CMakePresets.json` 进版本库，所有人、所有环境用同一套配置；个人临时改动放进**不提交**的 `CMakeUserPresets.json`，互不污染。
- **可组合、可复用**：用 `inherits` 让 debug/release/各平台 preset 继承同一个隐藏的"基础 preset"，公共字段只写一遍；用 `condition` 让某个 preset 只在特定平台启用。

### 16.1.3 与命令行参数的关系

Presets **不是**对命令行的替代而是对它的固化——`cmake --preset` 读取的字段最终仍然等价于对应的 `-D`/`-G`/`--toolchain` 等开关。你仍然可以在 `--preset` 之后追加额外的 `-D` 来覆盖 preset 中的某个缓存变量：

```bash
# 用 linux-debug 这个 preset，但临时把某个开关关掉
cmake --preset linux-debug -DMYPROJ_WITH_FOO=OFF
```

> ⚠️ presets 文件**只影响这五类操作**（configure / build / test / package / workflow），它不会改变 `CMakeLists.txt` 的语义，也不参与 `cmake -P` 脚本模式。

---

## 16.2 文件与版本

### 16.2.1 两个文件：项目级与用户级

Presets 分布在两个固定名字的文件里，二者都放在**顶层 `CMakeLists.txt` 所在目录**（即源码树根目录）：

| 文件 | 用途 | 是否提交版本库 |
|------|------|----------------|
| `CMakePresets.json` | 项目级、团队共享的官方 preset | ✅ 应提交 |
| `CMakeUserPresets.json` | 个人本机的私有 preset / 覆盖 | ❌ 应加入 `.gitignore` |

关键规则：**若两个文件都存在，`CMakeUserPresets.json` 会隐式 include `CMakePresets.json`**（任何 schema 版本都如此，无需写 `include` 字段）。因此用户文件里的 preset 可以直接 `inherits` 项目文件里的 preset。两个文件的同类 preset 名字在同一目录下**不能重名**（configure 之间不能重名，但一个 configure preset 与一个 build/test 同名是允许的）。

```gitignore
# .gitignore 推荐
CMakeUserPresets.json
```

### 16.2.2 顶层字段：`version` 与 `cmakeMinimumRequired`

每个 presets 文件的根对象有两个版本相关字段：

- **`version`**：**必填整数**，表示 presets **文件格式（schema）版本**。这是 presets 体系最容易踩坑的地方——它和 CMake 版本完全是两套数字。下表给出本教程涉及到的关键版本与对应 CMake 版本、引入的能力：

| schema `version` | 起始 CMake | 引入的关键能力 |
|:---:|:---:|------|
| 1 | 3.19 | `configurePresets`（最初版） |
| 2 | 3.20 | `buildPresets`、`testPresets` |
| 3 | 3.21 | `condition` 条件对象、`installDir`、`toolchainFile`、`$penv{}`、`${sourceParentDir}` 等 |
| 4 | 3.23 | `include` 字段（拆分文件）、`${fileDir}`、`$vendor{}` |
| 5 | 3.24 | `${pathListSep}` 宏、`trace` 字段 |
| 6 | 3.25 | **`packagePresets`、`workflowPresets`**、test `output.outputJUnitFile` |
| 7 | 3.27 | `include` 字段支持 `$penv{}` 宏展开 |
| 8 | 3.28 | configure preset 的 `trace` 调试字段细化 |
| 9 | 3.30 | `include` 字段支持除 `$env{}`/preset 专属宏外的全部宏展开 |
| 10 | 3.31 | `$comment` 字段（全文件可注释）、configure 的 `graphviz` 字段 |
| **11** | **4.3** | **test `execution.jobs` 允许空字符串**（等价 `--parallel` 不带数字） |

> 📌 **写 presets 时第一条铁律**：`version` 取你用到的**所有字段所要求的最高版本**。例如用了 `workflowPresets` 就至少要 `"version": 6`；如果只是基础 configure preset，`"version": 1` 即可获得最大兼容性。CMake 在解析时会拒绝 `version` 高于自身所支持上限的文件。

- **`cmakeMinimumRequired`**：可选对象，声明读取此文件所需的**最低 CMake 版本**，形如 `{ "major": 3, "minor": 25, "patch": 0 }`。它与 `version` 各司其职：`version` 决定能用哪些**字段**，`cmakeMinimumRequired` 决定需要哪个 **CMake 可执行程序**。

### 16.2.3 `$schema`：编辑器智能提示

可选的根字段 `$schema`（version 8 起官方手册正式收录该约定）指向 JSON Schema 文件，VS Code 等编辑器据此提供补全与校验：

```json
{
  "$schema": "https://cmake.org/cmake/help/latest/_downloads/schema.json",
  "version": 6
}
```

### 16.2.4 `include`：拆分文件（version ≥ 4）

大型项目可把不同平台/不同模块的 preset 拆到多个文件，再用根级 **`include`** 数组聚合（version 4 引入）。被包含的文件本身也可以再 `include`，形成树。一个 preset 只能 `inherits` 与它**定义在同一文件、或该文件（直接/间接）include 的文件**中的 preset。

```json
{
  "version": 6,
  "include": [
    "cmake/presets/base.json",
    "cmake/presets/linux.json",
    "cmake/presets/windows.json"
  ]
}
```

`include` 中的路径自 version 7 起支持 `$penv{}` 宏，自 version 9 起支持除 `$env{}` 与 preset 专属宏外的全部宏。

### 16.2.5 `$comment` 与 `vendor`

- **`$comment`**（version ≥ 10）：可放在根对象或任意 preset 对象里的任意位置，值任意（字符串/对象/数组），CMake 完全忽略它，纯供人阅读。这是在 JSON 里写注释的官方途径（JSON 本身无注释语法）。
- **`vendor`**：可选对象，供 IDE 等第三方写入私有信息。CMake 除校验它是对象外不解释其内容。键名约定用 `域名/工具/版本` 形式，例如 `"example.com/ExampleIDE/1.0"`。

---

## 16.3 configurePresets（配置预设）

`configurePresets` 是数组，每个元素描述一次"配置（configure）"——等价于一次 `cmake -S -B -G -D...`。这是 presets 体系的核心，其余四类 preset 都要引用某个 configure preset。下面逐字段详解。

### 16.3.1 标识与文档字段

- **`name`**：**必填字符串**，机器友好的唯一名字，就是 `cmake --preset <name>` 里的 `<name>`。同一目录的 configure preset 之间不可重名。
- **`displayName`**：可选字符串，给人看的友好名字，IDE 列表里显示的就是它。
- **`description`**：可选字符串，更长的说明文字。
- **`hidden`**：可选布尔。设为 `true` 的 preset **不能**被 `--preset` 直接使用，也无需提供完整有效的配置（即使靠继承也不必）。它的唯一用途是作为 `inherits` 的**基类**，把公共字段抽出来复用。这是组织 presets 的核心手法。

### 16.3.2 `inherits`：继承

- **`inherits`**：可选，字符串或字符串数组，列出要继承的 preset 名字。继承会拷入被继承 preset 的**全部字段**，但 `name`、`hidden`、`inherits`、`description`、`displayName` 这五个**不继承**。子 preset 可覆盖任意继承来的字段。

继承的两条细则：

1. **多继承冲突时，数组里靠前的 preset 优先**。`"inherits": ["a", "b"]` 中若 `a` 和 `b` 对同一字段给了不同值，取 `a` 的。
2. 只能继承**同文件或被 include 文件**中定义的 preset。

```json
{
  "name": "linux-debug",
  "inherits": "base-linux",
  "displayName": "Linux Debug",
  "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
}
```

### 16.3.3 `generator` 与目录字段

- **`generator`**：可选字符串，构建系统生成器名，等价于 `-G`，如 `"Ninja"`、`"Ninja Multi-Config"`、`"Unix Makefiles"`、`"Visual Studio 17 2022"`。
- **`binaryDir`**：可选字符串，构建（输出）目录，等价于 `-B`。**支持宏展开**；相对路径相对于**源码目录**解析。最常见写法是按 preset 名分目录，避免不同配置互相覆盖：

  ```json
  "binaryDir": "${sourceDir}/build/${presetName}"
  ```

- **`installDir`**（version ≥ 3）：可选字符串，安装前缀，等价于设置 `CMAKE_INSTALL_PREFIX`。**支持宏展开**；相对路径相对源码目录。

### 16.3.4 `cacheVariables`：缓存变量

- **`cacheVariables`**：可选对象，对应一堆 `-D`。每个键是缓存变量名，值有三种写法：

  - 直接给字符串：`"CMAKE_BUILD_TYPE": "Release"`（类型默认 `STRING`，布尔会被识别）。
  - 给对象指定类型：`{ "type": "BOOL", "value": "ON" }`，`type` 可为 `BOOL`/`FILEPATH`/`PATH`/`STRING`/`INTERNAL` 等。
  - 给 `null`：表示**移除**继承来的该变量（用于在子 preset 里"撤销"父 preset 设的缓存项）。

  值字符串**支持宏展开**。

```json
"cacheVariables": {
  "CMAKE_BUILD_TYPE": "Release",
  "BUILD_TESTING": { "type": "BOOL", "value": "ON" },
  "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
  "MYPROJ_DATA_DIR": "${sourceDir}/data"
}
```

### 16.3.5 `environment`：环境变量

- **`environment`**：可选对象，为本次 configure（及继承它的 build/test）设置环境变量。键是变量名，值是字符串或 `null`（`null` 表示从环境中移除）。值**支持宏展开**，且可以引用同一 `environment` 块里的其他变量（`$env{}`），也可引用父进程环境（`$penv{}`）。

```json
"environment": {
  "NINJA_STATUS": "[%f/%t] ",
  "PATH": "$env{MYTOOLS}/bin$penv{PATH}",
  "MYTOOLS": "${sourceDir}/tools"
}
```

> 上例中 `PATH` 里用 `$penv{PATH}` 引用**父进程**原有 PATH（避免 `$env{}` 自引用导致的循环），再前置自定义工具目录——这是 presets 里追加 PATH 的标准写法。`pathListSep` 宏可用于跨平台拼接（见 16.7）。

### 16.3.6 `toolchainFile`：工具链文件

- **`toolchainFile`**（version ≥ 3）：可选字符串，等价于 `--toolchain` / 设置 `CMAKE_TOOLCHAIN_FILE`。**支持宏展开**；相对路径**先相对构建目录、找不到再相对源码目录**解析。此字段优先级高于 `cacheVariables` 里直接写的 `CMAKE_TOOLCHAIN_FILE`。

```json
"toolchainFile": "${sourceDir}/cmake/toolchains/aarch64-linux.cmake"
```

### 16.3.7 `architecture` / `toolset`：平台与工具集（含 `strategy`）

- **`architecture`** 与 **`toolset`**：可选，对应 `-A`（平台架构）与 `-T`（工具集），主要服务于 Visual Studio 这类需要显式指定平台/工具集的生成器。两者各可写成**字符串**或**对象**：

  ```json
  "architecture": "x64",
  "toolset": "v143"
  ```

  对象形式额外带一个关键的 **`strategy`** 字段：

  - **`"strategy": "set"`**：由 CMake 真正去设置该值。对**不支持**该字段的生成器（如命令行的 Ninja）会**报错**。适合 Visual Studio 生成器。
  - **`"strategy": "external"`**：CMake **不设置**该值（即使生成器支持也不设），把字段留给外部 IDE 解释。典型场景：用 Ninja 生成器，但希望 Visual Studio 据 `architecture`/`toolset` 先把 MSVC 环境（vcvars）准备好再调 CMake——CMake 自身忽略，IDE 负责消费。

```json
{
  "name": "win-ninja-msvc",
  "generator": "Ninja Multi-Config",
  "architecture": { "value": "x64", "strategy": "external" },
  "toolset":      { "value": "v143,host=x64", "strategy": "external" },
  "cacheVariables": {
    "CMAKE_C_COMPILER": "cl.exe",
    "CMAKE_CXX_COMPILER": "cl.exe"
  }
}
```

> 支持 `architecture` 的 IDE 生成器见 `CMAKE_GENERATOR_PLATFORM`，支持 `toolset` 的见 `CMAKE_GENERATOR_TOOLSET`。

### 16.3.8 `condition`：启用条件

- **`condition`**（version ≥ 3）：可选，决定该 preset **是否启用**（不启用则不在 `--list-presets` 出现、不能被 `--preset` 选中）。可为布尔、`null` 或**条件对象**。`null` 表示启用，但**该条件不被继承**（子 preset 不会带上这个 `null`）。条件对象的完整语法见 16.8 节。最常见用途是按平台启停：

```json
"condition": {
  "type": "equals",
  "lhs": "${hostSystemName}",
  "rhs": "Linux"
}
```

### 16.3.9 `warnings` / `errors` / `debug`：诊断字段

- **`warnings`**：可选对象，开关各类警告，对应 `cmake` 的诊断开关：
  - `dev` / `deprecated`（布尔）：开发者警告 / 弃用警告。
  - `uninitialized`（布尔）：使用未初始化变量时警告（`--warn-uninitialized`）。
  - `unusedCli`（布尔，默认 `true`）：未被使用的命令行 `-D` 警告。
  - `systemVars`（布尔）：系统变量相关警告。
- **`errors`**：可选对象，把警告升级为错误：
  - `dev` / `deprecated`（布尔）：把开发者/弃用警告当错误。
- **`debug`**：可选对象，调试 CMake 本身：
  - `output`（布尔）：`--debug-output`。
  - `tryCompile`（布尔）：`--debug-trycompile`，保留 try_compile 临时目录。
  - `find`（布尔）：`--debug-find`，打印 `find_*` 查找过程。
- **`trace`**（version ≥ 5）：可选对象，控制 `--trace` 跟踪（`mode`、`format`、`source`、`redirect` 等子字段）。
- **`graphviz`**（version ≥ 10）：可选字符串，等价 `--graphviz=<file>`，输出依赖图。支持宏展开。

```json
"warnings": { "dev": true, "deprecated": true, "uninitialized": true },
"errors":   { "deprecated": true }
```

---

## 16.4 buildPresets（构建预设）

`buildPresets`（version ≥ 2）数组里的每个元素描述一次"构建"，等价于一次 `cmake --build`。

逐字段：

- **`name`** / `displayName` / `description` / `hidden` / `inherits` / `condition` / `environment` / `vendor`：与 configure preset 同义（命名空间独立：build preset 之间不能重名，但可与某 configure preset 同名）。
- **`configurePreset`**：**关键字段**，字符串，指明本 build 关联哪个 configure preset。非 hidden 的 build preset 必须能（直接或经继承）解析到一个 configure preset——构建目录、生成器都从它取。
- **`inheritConfigureEnvironment`**：可选布尔（默认 `true`），是否继承关联 configure preset 的 `environment`。
- **`targets`**：可选，字符串或字符串数组，要构建的目标，等价 `--target`。省略则构建默认目标。支持宏展开。
- **`jobs`**：可选整数，并行任务数，等价 `-j` / `--parallel`。
- **`configuration`**：可选字符串，多配置生成器（Ninja Multi-Config、Visual Studio）下选择构建哪个配置，等价 `--config`，如 `"Debug"` / `"Release"`。
- **`cleanFirst`**：可选布尔，构建前先 `clean`，等价 `--clean-first`。
- **`verbose`**：可选布尔，详细输出，等价 `--verbose`。
- **`nativeToolOptions`**：可选字符串数组，传给底层构建工具的额外参数，等价命令行 `--` 之后的内容。数组值支持宏展开。

```json
{
  "name": "linux-debug-build",
  "configurePreset": "linux-debug",
  "displayName": "Build (Linux Debug)",
  "jobs": 8,
  "targets": ["all"],
  "nativeToolOptions": ["-k", "0"]
}
```

> 对多配置生成器，常见做法是一个 configure preset 配多个 build preset，分别把 `configuration` 设成 `Debug`/`Release`，无需重复 configure。

---

## 16.5 testPresets（测试预设）

`testPresets`（version ≥ 2）数组里每个元素描述一次"测试运行"，等价于一次 `ctest`。

共有字段（`name`/`displayName`/`description`/`hidden`/`inherits`/`condition`/`environment`/`vendor`/`configurePreset`/`inheritConfigureEnvironment`）与 build preset 同义。`configurePreset` 同样是关联到某 configure preset 以确定测试在哪个构建目录跑。专属的三个子对象如下。

### 16.5.1 `output`：输出控制

- **`output`**：可选对象，映射 `ctest` 的输出相关开关：
  - **`outputOnFailure`**（布尔）：失败时打印测试输出，等价 `--output-on-failure`。**最常用**。
  - `shortProgress`（布尔）：`--progress` 紧凑进度。
  - `verbosity`（字符串）：`"default"` / `"verbose"`(-V) / `"extra"`(-VV)。
  - `quiet`（布尔）：`--quiet`。
  - `outputLogFile`（字符串）：`--output-log <file>`，支持宏展开。
  - **`outputJUnitFile`**（字符串，version ≥ 6）：`--output-junit <file>`，输出 JUnit XML，CI 集成常用。支持宏展开。
  - `labelSummary` / `subprojectSummary`（布尔，默认 `true`，设 `false` 即 `--no-label-summary` / `--no-subproject-summary`）。
  - `maxPassedTestOutputSize` / `maxFailedTestOutputSize`（整数）：通过/失败测试输出截断字节数。
  - `testOutputTruncation`（字符串）：截断策略（`tail`/`heads`/`middle`）。
  - `maxTestNameWidth`（整数）：测试名列宽。

### 16.5.2 `filter`：筛选测试

- **`filter`**：可选对象，决定跑哪些测试，含 `include` 与 `exclude` 两个子对象，各支持按名字正则与标签正则筛选：
  - **`include`**：
    - `name`（字符串）：只跑名字匹配此正则的测试，等价 `-R`。支持宏展开。
    - `label`（字符串）：只跑标签匹配此正则的测试，等价 `-L`。支持宏展开。
    - `index`（对象或字符串）：按测试编号区间筛选（`start`/`end`/`stride`/`specificTests`），或指向一个文件。
    - `useUnion`（布尔）：`-U`，名字与标签取并集。
  - **`exclude`**：
    - `name`（字符串）：排除名字匹配的测试，等价 `-E`。
    - `label`（字符串）：排除标签匹配的测试，等价 `-LE`。
    - `fixtures`（对象）：排除 fixture（`any`/`setup`/`cleanup` 正则）。

### 16.5.3 `execution`：执行控制

- **`execution`**：可选对象，控制运行方式：
  - **`jobs`**（整数；**version 11 起也接受空字符串**）：并行度，等价 `-j` / `--parallel`。空字符串表示 `--parallel` 不带数字（让 CTest 自行决定）——这正是 schema **version 11（CMake 4.3）** 引入的新行为。
  - `stopOnFailure`（布尔）：首个失败即停，等价 `--stop-on-failure`。
  - **`repeat`**（对象）：重复策略，等价 `--repeat <mode>:<count>`。必含：
    - `mode`（字符串）：`"until-fail"` / `"until-pass"` / `"after-timeout"`。
    - `count`（整数）：次数。
  - `noTestsAction`（字符串）：找不到测试时的行为，`"default"` / `"error"`(`--no-tests=error`) / `"ignore"`(`--no-tests=ignore`)。
  - `timeout`（整数）：全局超时秒数，等价 `--timeout`。
  - `scheduleRandom`（布尔）：随机调度，等价 `--schedule-random`。
  - `enableFailover`（布尔）：`--repeat-until-fail` 风格的容错（`--rerun-failed` 相关）。
  - `resourceSpecFile`（字符串）：资源规格文件，等价 `--resource-spec-file`。
  - `testLoad`（整数）：负载阈值，等价 `--test-load`。

```json
{
  "name": "linux-debug-test",
  "configurePreset": "linux-debug",
  "displayName": "Test (Linux Debug)",
  "output": { "outputOnFailure": true, "outputJUnitFile": "${sourceDir}/build/${presetName}/junit.xml" },
  "filter": { "exclude": { "label": "slow" } },
  "execution": { "jobs": 8, "stopOnFailure": false, "repeat": { "mode": "until-pass", "count": 3 } }
}
```

---

## 16.6 packagePresets（CPack）与 workflowPresets

### 16.6.1 packagePresets（version ≥ 6）

`packagePresets` 数组里每个元素描述一次打包，等价于一次 `cpack --preset`。共有字段同前；专属字段：

- **`configurePreset`**：关联的 configure preset（确定从哪个构建目录打包）。
- **`generators`**：可选字符串数组，CPack 生成器列表，等价 `cpack -G`，如 `["TGZ", "ZIP"]` / `["DEB"]` / `["NSIS"]`。
- **`configurations`**：可选字符串数组，要打包的配置（多配置生成器下），等价 `cpack -C`。
- **`packageName`** / **`packageVersion`** / **`packageDirectory`** / **`vendor`**：可选字符串，分别覆盖 `CPACK_PACKAGE_NAME` / `..._VERSION` / 输出目录 / `CPACK_PACKAGE_VENDOR`。支持宏展开。
- **`configFile`**：可选字符串，指定 CPack 配置文件路径（默认 `CPackConfig.cmake`）。支持宏展开。
- **`variables`**：可选对象，额外的 CPack 变量（键值对），等价 `cpack -D VAR=VALUE`。
- **`output`**：可选对象（`debug`/`verbose` 布尔）。

```json
{
  "name": "linux-package",
  "configurePreset": "linux-release",
  "displayName": "Package (Linux)",
  "generators": ["TGZ", "DEB"],
  "packageDirectory": "${sourceDir}/build/${presetName}/packages"
}
```

### 16.6.2 workflowPresets（version ≥ 6）

**workflow preset** 把 configure → build → test → package 这一串操作合并成**一条命令**，由 `cmake --workflow --preset <name>` 触发。这是 CMake 3.25 引入、用于 CI"一键流水线"的核心特性。

字段：

- **`name`** / `displayName` / `description` / `vendor`（注意：workflow preset **没有** `inherits`/`hidden`/`condition`/`environment`，它只是步骤的编排）。
- **`steps`**：**必填数组**，每个元素是一步：
  - **`type`**：`"configure"` / `"build"` / `"test"` / `"package"`。
  - **`name`**：对应类型 preset 的名字。

两条硬性规则：

1. **第一步必须是 `configure` 类型**。
2. **后续所有非 configure 步骤所引用 preset 的 `configurePreset`，必须等于第一步那个 configure preset 的名字**——即整条工作流绑定在同一次配置上。

```json
{
  "name": "ci-linux",
  "displayName": "CI · Linux full pipeline",
  "steps": [
    { "type": "configure", "name": "linux-release" },
    { "type": "build",     "name": "linux-release-build" },
    { "type": "test",      "name": "linux-release-test" },
    { "type": "package",   "name": "linux-package" }
  ]
}
```

> ⚠️ 早期（CMake 3.25–3.26）的 schema 在 workflow step 里用的键是 `preset` 而非 `name`，且当时只支持 configure/build/test。**自 CMake 4.3 / 现代 schema，统一用 `name`**，并支持 package 步骤。本教程基准 4.3.4 一律用 `name`。

---

## 16.7 宏与变量

presets 文件里的字符串字段（凡标注"支持宏展开"者）可使用 `${...}` 宏与 `$env{...}` 等环境引用。CMake 在解析 preset 时展开它们。

### 16.7.1 宏总表

| 宏 | 含义 | 引入版本 |
|------|------|:---:|
| `${sourceDir}` | 顶层 `CMakeLists.txt` 所在的源码目录 | 1 |
| `${sourceParentDir}` | 源码目录的父目录 | 3 |
| `${sourceDirName}` | 源码目录路径的最后一段（目录名） | 3 |
| `${presetName}` | 当前 preset 的 `name` | 1 |
| `${generator}` | 当前 preset 使用的生成器；build/test preset 取其关联 configure preset 的生成器 | 1 |
| `${hostSystemName}` | 宿主操作系统名（`uname -s` 结果，如 `Linux`/`Darwin`/`Windows`） | 3 |
| `${fileDir}` | 当前**这份 presets 文件**所在目录（用于 include 场景定位相对路径） | 4 |
| `${dollar}` | 一个字面量 `$` 字符 | 1 |
| `${pathListSep}` | 平台原生路径列表分隔符（Unix `:`、Windows `;`） | 5 |
| `$env{<VAR>}` | 环境变量：**先查本 preset 的 `environment`，再查父进程环境** | 1 |
| `$penv{<VAR>}` | 环境变量：**只查父进程环境**（绕过本 preset 的 environment，避免自引用循环） | 3 |
| `$vendor{<key>}` | 厂商扩展宏；CMake 自身不解析，遇到即报错——专供 IDE 在自己解析阶段替换 | 4 |

### 16.7.2 关键宏用法解读

- **`${presetName}` + `binaryDir`** 是黄金搭档：`"${sourceDir}/build/${presetName}"` 让每个 preset 自动落到独立构建目录。
- **`$env{}` vs `$penv{}`**：要在 `environment.PATH` 里"追加路径"时，**必须用 `$penv{PATH}`** 取父进程原值，否则 `$env{PATH}` 会引用正在定义的同名变量造成循环。
- **`${pathListSep}`** 用于跨平台拼接路径列表：

  ```json
  "environment": {
    "MY_SEARCH": "${sourceDir}/a${pathListSep}${sourceDir}/b"
  }
  ```

- **`${dollar}`** 用于输出字面 `$`，例如想让 `NINJA_STATUS` 里出现 `$`。
- **`$vendor{}`** 只能被理解它的 IDE 替换；用 `cmake` 命令行直接跑会因无法解析而失败——不要在通用 preset 里用。

---

## 16.8 condition 条件对象

`condition`（version ≥ 3，用于 configure/build/test/package preset）的对象形式靠 `type` 字段区分类型。子条件（出现在 `not`/`anyOf`/`allOf` 里的条件）不得为 `null`。

### 16.8.1 condition 类型总表

| `type` | 语义 | 必含字段 |
|--------|------|----------|
| `const` | 常量布尔（等价直接写布尔值） | `value`（布尔） |
| `equals` | 两字符串相等 | `lhs`、`rhs`（均支持宏） |
| `notEquals` | 两字符串不等 | `lhs`、`rhs` |
| `inList` | 字符串在列表中 | `string`、`list`（数组，均支持宏；短路求值） |
| `notInList` | 字符串不在列表中 | `string`、`list` |
| `matches` | 字符串匹配正则 | `string`、`regex`（均支持宏） |
| `notMatches` | 字符串不匹配正则 | `string`、`regex` |
| `anyOf` | 子条件**任一**为真（逻辑或） | `conditions`（条件对象数组；短路） |
| `allOf` | 子条件**全部**为真（逻辑与） | `conditions`（条件对象数组；短路） |
| `not` | 对单个子条件取反 | `condition`（一个条件对象） |

> `lhs`/`rhs`/`string`/`regex`/`list` 中的字符串均支持宏展开，因此条件几乎总是写成"宏 vs 常量"的形式。

### 16.8.2 按平台启用：常见模式

只在 Windows 启用：

```json
"condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
```

在类 Unix（Linux 或 macOS）启用——用 `inList`：

```json
"condition": {
  "type": "inList",
  "string": "${hostSystemName}",
  "list": ["Linux", "Darwin"]
}
```

组合条件（在 Linux **且** 某环境变量被设置时启用）——用 `allOf`：

```json
"condition": {
  "type": "allOf",
  "conditions": [
    { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux" },
    { "type": "notEquals", "lhs": "$penv{CI}", "rhs": "" }
  ]
}
```

用正则匹配（preset 名包含 `release`）：

```json
"condition": { "type": "matches", "string": "${presetName}", "regex": ".*release.*" }
```

---

## 16.9 命令行

presets 在各工具中的调用入口：

```bash
# 配置：用某 configure preset
cmake --preset <configure-preset>

# 构建：用某 build preset（注意是 --build --preset，不是 --preset）
cmake --build --preset <build-preset>

# 测试：用某 test preset
ctest --preset <test-preset>

# 打包：用某 package preset
cpack --preset <package-preset>

# 工作流：一键串联 configure→build→test→package
cmake --workflow --preset <workflow-preset>
```

辅助命令：

```bash
# 列出可用的 configure preset（受 condition 过滤，被禁用的不显示）
cmake --list-presets

# 列出指定类别的 preset
cmake --list-presets=build      # 也可 configure / test / package / all
ctest --list-presets            # 列出 test preset
cpack --list-presets            # 列出 package preset
cmake --workflow --list-presets # 列出 workflow preset

# 从非默认位置读取 presets 文件
ctest --preset foo --presets-file path/to/CMakePresets.json
```

覆盖与叠加：

```bash
# 用 preset，但临时追加/覆盖缓存变量
cmake --preset linux-debug -DMYPROJ_WITH_FOO=OFF

# 用 preset，但换个构建目录（命令行 -B 覆盖 preset 的 binaryDir）
cmake --preset linux-debug -B /tmp/alt-build
```

> 💡 `cmake --build --preset` 与 `cmake --preset` 的区别要记牢：前者是**构建**（需 `--build`），后者是**配置**。漏掉 `--build` 会被当成配置 preset 而报"找不到 configure preset"。

---

## 16.10 完整示例

下面是一份覆盖 **Linux / Windows** 的 `CMakePresets.json`：含一个隐藏的公共基础 preset、各平台基础 preset（带 `condition`）、debug/release 派生、对应的 build/test/package preset，以及把它们串起来的 workflow preset。

```json
{
  "$schema": "https://cmake.org/cmake/help/latest/_downloads/schema.json",
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },

  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "$comment": "所有 preset 的公共根：统一构建目录布局与通用缓存项",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "installDir": "${sourceDir}/install/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "BUILD_TESTING": { "type": "BOOL", "value": "ON" }
      },
      "warnings": { "dev": true, "deprecated": true }
    },

    {
      "name": "linux-base",
      "hidden": true,
      "inherits": "base",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux" },
      "cacheVariables": {
        "CMAKE_C_COMPILER": "clang",
        "CMAKE_CXX_COMPILER": "clang++"
      }
    },
    {
      "name": "linux-debug",
      "inherits": "linux-base",
      "displayName": "Linux · Debug",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "linux-release",
      "inherits": "linux-base",
      "displayName": "Linux · Release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    },

    {
      "name": "win-base",
      "hidden": true,
      "inherits": "base",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" },
      "generator": "Ninja Multi-Config",
      "architecture": { "value": "x64", "strategy": "external" },
      "toolset":      { "value": "host=x64", "strategy": "external" },
      "cacheVariables": {
        "CMAKE_C_COMPILER": "cl.exe",
        "CMAKE_CXX_COMPILER": "cl.exe"
      }
    },
    {
      "name": "win-x64",
      "inherits": "win-base",
      "displayName": "Windows · x64 (Multi-Config)"
    }
  ],

  "buildPresets": [
    { "name": "linux-debug",   "configurePreset": "linux-debug",   "displayName": "Build · Linux Debug",   "jobs": 8 },
    { "name": "linux-release", "configurePreset": "linux-release", "displayName": "Build · Linux Release", "jobs": 8 },
    { "name": "win-debug",     "configurePreset": "win-x64",       "displayName": "Build · Win Debug",   "configuration": "Debug" },
    { "name": "win-release",   "configurePreset": "win-x64",       "displayName": "Build · Win Release", "configuration": "Release" }
  ],

  "testPresets": [
    {
      "name": "linux-release",
      "configurePreset": "linux-release",
      "displayName": "Test · Linux Release",
      "output":    { "outputOnFailure": true, "outputJUnitFile": "${sourceDir}/build/${presetName}/junit.xml" },
      "execution": { "jobs": 8, "stopOnFailure": false }
    },
    {
      "name": "win-release",
      "configurePreset": "win-x64",
      "displayName": "Test · Win Release",
      "configuration": "Release",
      "output":    { "outputOnFailure": true },
      "execution": { "jobs": 8 }
    }
  ],

  "packagePresets": [
    {
      "name": "linux-release",
      "configurePreset": "linux-release",
      "displayName": "Package · Linux",
      "generators": ["TGZ", "DEB"]
    },
    {
      "name": "win-release",
      "configurePreset": "win-x64",
      "displayName": "Package · Win",
      "configurations": ["Release"],
      "generators": ["ZIP", "NSIS"]
    }
  ],

  "workflowPresets": [
    {
      "name": "ci-linux",
      "displayName": "CI · Linux (configure→build→test→package)",
      "steps": [
        { "type": "configure", "name": "linux-release" },
        { "type": "build",     "name": "linux-release" },
        { "type": "test",      "name": "linux-release" },
        { "type": "package",   "name": "linux-release" }
      ]
    },
    {
      "name": "ci-windows",
      "displayName": "CI · Windows",
      "steps": [
        { "type": "configure", "name": "win-x64" },
        { "type": "build",     "name": "win-release" },
        { "type": "test",      "name": "win-release" },
        { "type": "package",   "name": "win-release" }
      ]
    }
  ]
}
```

配套命令——本机是 Linux 时：

```bash
# 单步走
cmake --preset linux-debug          # 配置
cmake --build --preset linux-debug  # 构建
ctest --preset linux-release        # 测试（先确保已配置/构建 release）
cpack --preset linux-release        # 打包

# 一键 CI 流水线（自动 configure→build→test→package）
cmake --workflow --preset ci-linux
```

本机是 Windows 时（`win-*` preset 因 `condition` 而启用，`linux-*` 自动隐藏）：

```bash
cmake --preset win-x64
cmake --build --preset win-release
ctest --preset win-release
cmake --workflow --preset ci-windows
```

> 由于 `linux-base` 与 `win-base` 各带 `condition`，在 Linux 上 `cmake --list-presets` 只会看到 `linux-debug`/`linux-release`，Windows 上只看到 `win-x64`——同一份文件，自动适配平台。

个人临时覆盖放进 **`CMakeUserPresets.json`**（不提交）：

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "my-linux-debug",
      "inherits": "linux-debug",
      "displayName": "[私有] 我的 Linux Debug",
      "cacheVariables": { "MYPROJ_VERBOSE_LOG": "ON" },
      "environment": { "ASAN_OPTIONS": "detect_leaks=1" }
    }
  ]
}
```

```bash
cmake --preset my-linux-debug   # 用户文件隐式 include 了项目文件，可直接 inherits
```

---

## 16.11 跨平台 Presets 实例

上一节的完整示例覆盖了 Linux / Windows，本节再补一份**同时覆盖 Windows / macOS / Linux** 三平台的精炼实例，聚焦一个核心手法：**用一份 `CMakePresets.json`，靠 `condition` 的 `${hostSystemName}` 自动派生出当前平台该用的 preset**——开发者在三台机器上敲的是同一组 preset 名，CMake 根据宿主系统挑出唯一启用的那个。

### 16.11.1 一份文件覆盖三平台

要点：一个隐藏的 `cross-base` 抽出公共字段（构建目录布局、通用缓存项）；三个平台基础 preset 各带 `condition`（`equals` + `${hostSystemName}`，三平台分别匹配 `Windows`/`Darwin`/`Linux`）并设各自合适的 generator；再派生出对外可用的 `windows-x64`/`mac-arm64`/`linux-debug`。`${hostSystemName}` 取的是 `uname -s` 风格的名字，macOS 是 **`Darwin`** 而非 `macOS`——这是最易踩的坑。

```json
{
  "$schema": "https://cmake.org/cmake/help/latest/_downloads/schema.json",
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },

  "configurePresets": [
    {
      "name": "cross-base",
      "hidden": true,
      "$comment": "三平台公共根：统一构建目录与通用缓存项",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "installDir": "${sourceDir}/install/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "BUILD_TESTING": { "type": "BOOL", "value": "ON" }
      },
      "warnings": { "dev": true, "deprecated": true }
    },

    {
      "name": "windows-x64",
      "inherits": "cross-base",
      "displayName": "Windows · x64 (Visual Studio)",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" },
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "cacheVariables": { "CMAKE_CONFIGURATION_TYPES": "Debug;Release" }
    },
    {
      "name": "mac-arm64",
      "inherits": "cross-base",
      "displayName": "macOS · arm64 (Ninja)",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Darwin" },
      "generator": "Ninja",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_OSX_ARCHITECTURES": "arm64"
      }
    },
    {
      "name": "linux-debug",
      "inherits": "cross-base",
      "displayName": "Linux · Debug (Ninja)",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux" },
      "generator": "Ninja",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    }
  ],

  "buildPresets": [
    { "name": "windows-x64", "configurePreset": "windows-x64", "displayName": "Build · Windows", "configuration": "Release" },
    { "name": "mac-arm64",   "configurePreset": "mac-arm64",   "displayName": "Build · macOS",   "jobs": 8 },
    { "name": "linux-debug", "configurePreset": "linux-debug", "displayName": "Build · Linux",   "jobs": 8 }
  ],

  "testPresets": [
    { "name": "windows-x64", "configurePreset": "windows-x64", "configuration": "Release", "output": { "outputOnFailure": true } },
    { "name": "mac-arm64",   "configurePreset": "mac-arm64",   "output": { "outputOnFailure": true }, "execution": { "jobs": 8 } },
    { "name": "linux-debug", "configurePreset": "linux-debug", "output": { "outputOnFailure": true }, "execution": { "jobs": 8 } }
  ]
}
```

> 注意：Visual Studio 是多配置生成器，故 `windows-x64` 用 `CMAKE_CONFIGURATION_TYPES` 而非 `CMAKE_BUILD_TYPE`，并在 build/test preset 里用 `configuration` 选 `Release`；mac/Linux 用单配置的 Ninja，直接以 `CMAKE_BUILD_TYPE` 定档。

### 16.11.2 各平台调用命令

三平台的 preset 名彼此不同，但因 `condition` 互斥，每台机器上只有对应那个可见、可用：

```bash
# Windows（仅 windows-x64 启用）
cmake --preset windows-x64
cmake --build --preset windows-x64

# macOS（仅 mac-arm64 启用）
cmake --preset mac-arm64
cmake --build --preset mac-arm64

# Linux（仅 linux-debug 启用）
cmake --preset linux-debug
cmake --build --preset linux-debug
```

### 16.11.3 `condition` 区分平台的写法要点

- 三平台用同一套模板：`{ "type": "equals", "lhs": "${hostSystemName}", "rhs": "<系统名>" }`，`<系统名>` 三平台分别取 `Windows` / `Darwin` / `Linux`。
- `lhs` 写宏 `${hostSystemName}`、`rhs` 写常量字符串——`condition` 几乎总是这种"宏 vs 常量"的形式。
- 想"类 Unix（mac + Linux）共用一个 preset"时，把 `equals` 换成 `inList`：`{ "type": "inList", "string": "${hostSystemName}", "list": ["Darwin", "Linux"] }`（见 16.8.2）。
- 平台名大小写敏感，且 macOS 必须写 `Darwin`——写成 `macOS`/`Mac` 会导致该 preset 永远不启用。

> 更完整的跨平台工程（含 `workflowPresets` 一键流水线、按平台分文件 `include`、工具链文件等）见 [[42.Cmake/19 - 跨平台实战与平台差异大全.md|第 19 章]]。

## 本章小结

- **Presets 用 JSON 固化配置**，让 CLI / IDE / CI 共享同一入口，根治 `-D` 漂移与"换台机器重配"的痛点。
- **两个文件**：`CMakePresets.json`（提交）+ `CMakeUserPresets.json`（私有、隐式 include 前者）；根字段 `version`（**文件 schema 版本**，与 CMake 版本是两套数字）+ `cmakeMinimumRequired`；`include` 拆分文件（≥4）。**用到的最高字段决定 `version`**——本章基准下，`workflowPresets`/`packagePresets` 需 ≥6，`testPresets.execution.jobs` 空串需 **11（CMake 4.3）**。
- **五类 preset**：`configurePresets`（核心，含 `inherits`/`hidden`/`cacheVariables`/`environment`/`toolchainFile`/`architecture`+`toolset`+`strategy`/`condition`）、`buildPresets`、`testPresets`（`output`/`filter`/`execution`）、`packagePresets`（≥6）、`workflowPresets`（≥6，`steps` 串联，首步必为 configure）。
- **宏**（`${sourceDir}`/`${presetName}`/`${hostSystemName}`/`${fileDir}`/`${pathListSep}`/`$env{}`/`$penv{}` 等）+ **condition 对象**（`equals`/`inList`/`matches`/`anyOf`/`allOf`/`not` 等）是实现"一份文件、多平台自适应"的两大杠杆。
- **命令**：`cmake --preset`、`cmake --build --preset`、`ctest --preset`、`cpack --preset`、`cmake --workflow --preset`、`cmake --list-presets`——务必区分配置 (`--preset`) 与构建 (`--build --preset`)。

---

> ⬅️ [[42.Cmake/15 - 生成器与构建系统.md|上一章]] ｜ ➡️ [[42.Cmake/17 - 模块与常用工具模块.md|第 17 章]]
>
> 🏠 [[00 - CMake 完整技术教程 - 总索引|总索引]]
