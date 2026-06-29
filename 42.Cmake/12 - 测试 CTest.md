---
title: 测试 CTest
tags: [cmake, ctest, testing, reference]
chapter: "12"
cmake_version: 4.3.4
---

# 第 12 章 · 测试 CTest

> 基准版本：CMake 4.3.4

测试是工程质量的底线。CMake 自带一套与构建系统深度集成的测试驱动器——**CTest**。本章从 `enable_testing()` / `add_test()` 这两条核心命令出发，逐项讲解测试属性、`ctest` 命令行、与 GoogleTest / Catch2 等框架的集成、fixtures 编排、CTest 脚本与 CDash Dashboard，以及内存检查与覆盖率。

本章所有特性均以 CMake 4.3.4 为基准核对官方手册（`ctest(1)`、`add_test`、`set_tests_properties`、`module/GoogleTest`）。

---

## 12.1 CTest 概述

### 12.1.1 CTest 是什么

CTest 是随 CMake 一同安装的独立可执行程序（`ctest`，Windows 上为 `ctest.exe`）。它的职责是 **运行测试、收集结果、汇报通过/失败**，并可把结果提交到 CDash 仪表盘。它在构建目录中读取由 CMake 生成的 `CTestTestfile.cmake` 文件，按其中登记的测试逐条执行。

CTest 的工作模型可以一句话概括：

> **CTest 把"一条测试"等同于"运行一个命令，并根据其退出码（默认 0 = 通过，非 0 = 失败）判定结果"。**

在此基础上，CTest 叠加了超时、标签、依赖、正则匹配输出、并行调度、资源锁等一整套属性，使其足以胜任从单元测试到端到端集成测试的调度工作。

### 12.1.2 CTest 与单元测试框架的关系（关键概念）

初学者最常见的误解是把 CTest 当成"断言库"，例如期待它提供 `ASSERT_EQ` 之类的宏。**这是错的。** 必须厘清两层职责：

| 角色 | 代表 | 职责 |
| :--- | :--- | :--- |
| **断言库 / 测试框架** | GoogleTest、Catch2、doctest、Boost.Test、Qt Test | 提供 `EXPECT_EQ` / `REQUIRE` 等断言宏、测试用例组织（`TEST()` / `TEST_CASE()`）、断言失败时打印诊断信息、决定单个测试可执行程序的退出码 |
| **测试调度器 / 驱动器** | **CTest** | 不关心断言怎么写。它只负责"运行哪些可执行程序、用什么参数、以什么顺序、并行多少、超时多久、失败如何重跑、结果如何汇总和上报" |

二者是**互补**而非竞争关系。典型分工：

- 你用 **GoogleTest** 写 `TEST(MathTest, Add) { EXPECT_EQ(add(2,3), 5); }`，编译成一个测试可执行程序；这个程序自己会在断言失败时返回非 0 退出码。
- 你用 **CTest** 把这个可执行程序（或它内部的每一个 `TEST`）登记为测试用例，然后 `ctest -j8 --output-on-failure` 一键并行跑完、汇总。

CTest 也可以驱动**完全不依赖任何框架**的测试——例如直接运行一个返回退出码的脚本，或运行你的命令行程序并用正则匹配它的 stdout。这就是为什么"光有 CTest、不引入框架"也能做基本的黑盒测试。

---

## 12.2 启用与定义测试

### 12.2.1 `enable_testing()`：开启测试

要让 CTest 在某个构建目录生效，必须调用 `enable_testing()`。

```cmake
cmake_minimum_required(VERSION 4.3)
project(MyApp LANGUAGES CXX)

enable_testing()          # 关键：开启当前目录及其所有子目录的测试
add_subdirectory(tests)
```

要点：

- **`enable_testing()` 必须在顶层 `CMakeLists.txt` 中调用**（通常紧跟 `project()` 之后）。它会令 CMake 在**当前目录及全部子目录**生成 `CTestTestfile.cmake`，从而使 `add_test()` 在任意子目录都能被收集。
- 若只在某个子目录调用 `enable_testing()`，则其上级目录不会生成测试入口，`ctest` 在构建根运行时可能"看不到"测试——这是新手最常踩的坑。
- 一个等价且更强大的替代写法是 `include(CTest)`（见 §12.8.3），它会**自动调用 `enable_testing()`** 并额外提供 `BUILD_TESTING` 选项与 Dashboard 支持。二者**择一即可**，不要重复。

### 12.2.2 `add_test()`：定义一条测试

`add_test()` 有两种形式，强烈推荐使用**新式（NAME/COMMAND）形式**。

**新式形式（推荐）：**

```cmake
add_test(NAME <name>
         COMMAND <command> [<arg>...]
         [CONFIGURATIONS <config>...]
         [WORKING_DIRECTORY <dir>]
         [COMMAND_EXPAND_LISTS])
```

- `<name>`：测试在 CTest 中的唯一名字（用于 `-R` / `-E` 过滤、报表显示）。可含字母数字与下划线，**不要含空格**。
- `COMMAND <command>`：要运行的命令。`<command>` 可以是：
  - 一个 **CMake 目标名**（由 `add_executable()` 定义）——CMake 会自动替换为该目标的最终可执行文件路径，这是最佳实践；
  - 一个 `$<TARGET_FILE:...>` 等生成器表达式；
  - 一个外部程序的绝对/相对路径，如 `${CMAKE_COMMAND}`、`python3`。
- `WORKING_DIRECTORY <dir>`：测试运行时的工作目录（不指定则为构建目录中对应的 `CTestTestfile.cmake` 所在目录）。
- `COMMAND_EXPAND_LISTS`：将命令中的 `;` 分隔列表展开为多个参数。

**旧式形式（仅作了解，不推荐新代码使用）：**

```cmake
add_test(<name> <exename> [<arg>...])
```

旧式形式不支持 `WORKING_DIRECTORY`、`CONFIGURATIONS` 等关键字，且 `<exename>` 不会自动识别目标名（虽然 CMake 会尝试把已知目标名替换为可执行路径），可读性与功能都弱于新式。**新项目一律用新式。**

### 12.2.3 用目标名 / `$<TARGET_FILE:t>` 引用被测程序

为什么不直接写死可执行文件路径？因为多配置生成器（Visual Studio、Ninja Multi-Config、Xcode）会把产物放进 `Debug/` `Release/` 等子目录，硬编码路径必然在某些配置下失效。正确做法是让 CMake 在生成阶段替换真实路径。

最简单的写法——直接传**目标名**：

```cmake
add_executable(my_tests test_main.cpp)
add_test(NAME my_tests COMMAND my_tests)   # CMake 自动展开为 my_tests 的可执行路径
```

当需要把被测程序作为**参数**（而非作为命令本身）传递时，必须用生成器表达式 `$<TARGET_FILE:tgt>`，它在生成期求值为目标的完整可执行文件绝对路径：

```cmake
add_executable(calc calc.cpp)            # 被测的命令行程序
add_executable(runner runner.cpp)        # 测试驱动程序

# 把 calc 的真实路径作为 runner 的参数传入
add_test(NAME calc_via_runner
         COMMAND runner --target $<TARGET_FILE:calc>)
```

常用的目标相关生成器表达式：

- `$<TARGET_FILE:tgt>`：目标主产物的完整路径（含文件名）。
- `$<TARGET_FILE_DIR:tgt>`：目标产物所在目录。
- `$<TARGET_FILE_NAME:tgt>`：仅文件名。

一个常见组合——用 CMake 自身作为"断言器"比较程序输出：

```cmake
add_test(NAME output_matches
         COMMAND ${CMAKE_COMMAND}
                 -DTEST_PROG=$<TARGET_FILE:calc>
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/run_and_check.cmake)
```

---

## 12.3 测试属性（`set_tests_properties`）

定义测试后，用 `set_tests_properties()` 调整其行为。语法：

```cmake
set_tests_properties(<test1> [<test2>...]
                     [DIRECTORY <dir>]
                     PROPERTIES <prop1> <value1> [<prop2> <value2>...])
```

> 提示：**测试属性值支持生成器表达式**，因此可以写 `ENVIRONMENT "DATA=$<TARGET_FILE_DIR:foo>"` 这类依赖目标路径的值。

下表汇总本章涉及的全部常用测试属性。

| 属性 | 取值 | 作用简述 |
| :--- | :--- | :--- |
| `WILL_FAIL` | `TRUE`/`FALSE` | 反转通过判定：退出码非 0 才算通过 |
| `TIMEOUT` | 秒数（浮点） | 单条测试超时上限，超时即判失败并被杀死 |
| `LABELS` | 字符串列表 | 给测试打标签，配合 `ctest -L` 分组运行 |
| `DEPENDS` | 测试名列表 | 指定本测试在哪些测试**之后**运行（仅排序，不传递通过性） |
| `FIXTURES_SETUP` | fixture 名列表 | 声明本测试是某 fixture 的"建立"步骤 |
| `FIXTURES_CLEANUP` | fixture 名列表 | 声明本测试是某 fixture 的"清理"步骤 |
| `FIXTURES_REQUIRED` | fixture 名列表 | 声明本测试需要某 fixture，自动拉起对应 setup/cleanup |
| `ENVIRONMENT` | `VAR=value` 列表 | 为本测试设置环境变量（覆盖式） |
| `ENVIRONMENT_MODIFICATION` | `VAR=op:value` 列表 | 对环境变量做增量修改（追加/前置路径等） |
| `WORKING_DIRECTORY` | 目录路径 | 测试运行时的工作目录 |
| `PASS_REGULAR_EXPRESSION` | 正则列表 | 输出匹配任一正则才算通过 |
| `FAIL_REGULAR_EXPRESSION` | 正则列表 | 输出匹配任一正则即算失败 |
| `SKIP_REGULAR_EXPRESSION` | 正则列表 | 输出匹配即标记为"跳过"（4.x 仍有效） |
| `RUN_SERIAL` | `TRUE`/`FALSE` | 本测试必须独占运行，不与其他测试并行 |
| `PROCESSORS` | 整数 | 声明本测试占用的 CPU 数，影响 `-j` 调度 |
| `RESOURCE_LOCK` | 资源名列表 | 持有同名锁的测试互斥串行 |
| `RESOURCE_GROUPS` | 资源组描述 | 细粒度声明所需硬件资源（GPU 等），配合资源规格文件 |
| `SKIP_RETURN_CODE` | 0–255 整数 | 测试返回该退出码时记为"跳过"而非失败 |
| `DISABLED` | `TRUE`/`FALSE` | 禁用测试：不运行、不计入失败，仅标记 |
| `COST` | 浮点数 | 调度成本提示，CTest 优先调度高 cost 的测试 |

下面逐条详解。

### 12.3.1 通过/失败判定类

**`WILL_FAIL`**：反转标准的通过/失败判定。设为 `TRUE` 时，原本退出码为 0（成功）的测试反被判为**失败**，退出码非 0 反被判为**通过**。用于验证"程序在错误输入下确实会报错退出"的场景。注意：它**不覆盖超时**——超过 `TIMEOUT` 仍判失败；段错误、`abort`、堆损坏等系统级崩溃通常也仍算失败。

```cmake
add_test(NAME rejects_bad_input COMMAND calc --bad-flag)
set_tests_properties(rejects_bad_input PROPERTIES WILL_FAIL TRUE)
```

**`PASS_REGULAR_EXPRESSION`**：正则表达式列表。只要测试输出（stdout+stderr）匹配其中**任意一个**正则，即判为通过——此时**退出码被忽略**。

**`FAIL_REGULAR_EXPRESSION`**：正则列表。输出匹配**任意一个**即判为失败（优先级高于 `PASS_*`）。常用于捕捉 `ERROR`、`Segmentation fault`、`leak` 等关键字。

```cmake
add_test(NAME greets COMMAND hello)
set_tests_properties(greets PROPERTIES
  PASS_REGULAR_EXPRESSION "Hello, World"
  FAIL_REGULAR_EXPRESSION "ERROR;FATAL;abort")
```

**`SKIP_REGULAR_EXPRESSION`**：正则列表。输出匹配时该测试不算通过也不算失败，而是标记为 **Skipped**。适合测试程序自报"当前环境缺少依赖，跳过"的情形。

**`SKIP_RETURN_CODE`**：整数（0–255）。当测试进程以该退出码结束时，CTest 记为 **Skipped** 而非失败。许多框架约定用退出码 `77`（GNU Automake 传统）表示跳过。

```cmake
set_tests_properties(gpu_test PROPERTIES SKIP_RETURN_CODE 77)
```

### 12.3.2 时间与禁用类

**`TIMEOUT`**：浮点秒数。单条测试运行超过该时长即被强制终止并判失败。它会覆盖 `ctest --timeout` 给出的全局默认值。对可能死循环或挂起的测试务必设置。

```cmake
set_tests_properties(slow_io PROPERTIES TIMEOUT 30)   # 30 秒上限
```

**`DISABLED`**：设为 `TRUE` 时该测试被禁用——**完全不运行**，在结果中标记为 `Disabled`，且**不计入失败总数**。比 `WILL_FAIL` 更彻底，适合临时屏蔽已知坏掉、尚未修复的测试（优于直接注释掉 `add_test`，因为仍可见、可统计）。

```cmake
set_tests_properties(flaky_net_test PROPERTIES DISABLED TRUE)
```

**`COST`**：浮点数，调度成本提示。CTest 在并行运行时倾向于**先调度 cost 高（耗时长）的测试**，以缩短整批的墙钟时间。首次运行后 CTest 会自动记录各测试实际耗时作为后续 cost，手动设置主要用于首跑前的提示。

### 12.3.3 分组与排序类

**`LABELS`**：字符串列表，给测试贴标签。配合 `ctest -L <regex>`（包含）/ `-LE <regex>`（排除）按标签批量运行。是组织大型测试套件的核心手段。

```cmake
set_tests_properties(unit_math unit_str PROPERTIES LABELS "unit")
set_tests_properties(e2e_login          PROPERTIES LABELS "integration;slow")
```

```bash
ctest -L unit            # 只跑带 unit 标签的
ctest -LE slow           # 排除 slow 标签
```

**`DEPENDS`**：测试名列表，约束**运行顺序**——保证本测试在所列测试之后启动。**注意：`DEPENDS` 只影响顺序，不传递通过性**：即使被依赖的测试失败了，依赖它的测试**仍会运行**。若你想表达"前置失败则后续不必跑"的语义，应改用 fixtures（§12.6），fixture 的 setup 失败会令所有 `FIXTURES_REQUIRED` 它的测试被标记为 **Not Run**。

```cmake
set_tests_properties(dependsTest12 PROPERTIES DEPENDS "baseTest1;baseTest2")
```

### 12.3.4 环境与工作目录类

**`WORKING_DIRECTORY`**：测试进程的工作目录。也可在 `add_test(... WORKING_DIRECTORY ...)` 中直接给出；此属性用于事后修改或批量设置。

**`ENVIRONMENT`**：`VAR=value` 形式的列表，为该测试进程**设置（覆盖）**环境变量。

```cmake
set_tests_properties(db_test PROPERTIES
  ENVIRONMENT "DB_HOST=localhost;DB_PORT=5432;LANG=C")
```

**`ENVIRONMENT_MODIFICATION`**：对环境变量做**增量修改**，语法为 `NAME=OP:VALUE`。相比 `ENVIRONMENT` 的整体覆盖，它能精确地"在已有 `PATH` 上追加一项"。常用操作符：

| 操作符 | 含义 |
| :--- | :--- |
| `set` | 设为 VALUE（等价覆盖） |
| `unset` | 删除该变量（VALUE 忽略） |
| `string_append` / `string_prepend` | 在原值尾部/头部拼接字符串 |
| `path_list_append` / `path_list_prepend` | 以平台路径分隔符（`;`/`:`）追加/前置一项 |
| `cmake_list_append` / `cmake_list_prepend` | 以 `;` 作为 CMake 列表分隔符追加/前置 |

```cmake
set_tests_properties(plugin_test PROPERTIES
  ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:plugin_dll>;\
LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:plugin_so>")
```

上例的典型用途：让测试在运行时能找到刚构建出来的动态库（Windows 加到 `PATH`，Linux 加到 `LD_LIBRARY_PATH`），无需安装。

### 12.3.5 并行与资源调度类

**`RUN_SERIAL`**：设为 `TRUE` 时，该测试**独占运行**——CTest 在并行模式（`-j`）下也保证它运行期间没有其他测试同时进行。用于会争抢全局资源（如绑定固定端口、写同一文件）的测试。

**`PROCESSORS`**：整数，声明该测试运行时占用的处理器数量（默认 1）。CTest 的 `-j <N>` 是"同时使用的处理器总数"的预算，调度时会把各运行中测试的 `PROCESSORS` 相加，使之不超过 `N`。因此一个声明 `PROCESSORS 4` 的测试在 `-j8` 下最多与占 4 的其他测试并行。

```cmake
set_tests_properties(parallel_solver PROPERTIES PROCESSORS 4)
```

**`RESOURCE_LOCK`**：字符串（资源名）列表。**持有相同锁名的测试之间互斥**，不会同时运行（但与持其他锁或无锁的测试可并行）。比 `RUN_SERIAL` 更精细——它只串行化"争用同一资源"的那一组。

```cmake
# 这几个测试都访问同一个数据库，给它们加同名锁，保证不并发
set_tests_properties(dbOnly dbWithFoo createDB cleanupDB
                     PROPERTIES RESOURCE_LOCK DbAccess)
```

**`RESOURCE_GROUPS`**：CTest 的**资源分配**机制，用于声明测试需要的硬件资源（如多块 GPU、若干 slot），由 CTest 在运行时按一份"资源规格文件"（JSON）做分配，并通过环境变量告知测试拿到了哪些资源。它比 `RESOURCE_LOCK` 强大得多：锁只能表达"互斥"，而资源组能表达"我需要 2 个 GPU、每个至少 1 个计算单元"。

```cmake
# 该测试需要 1 组、组内 2 个名为 "gpus" 的资源、每个 slot 数为 2
set_tests_properties(multi_gpu_test PROPERTIES
  RESOURCE_GROUPS "2,gpus:2")
```

运行时需通过 `ctest --resource-spec-file <file>` 或环境变量 `CTEST_RESOURCE_SPEC_FILE` 提供资源清单。一份最小资源规格文件示例：

```json
{
  "version": { "major": 1, "minor": 0 },
  "local": [{
    "gpus": [
      { "id": "0", "slots": 4 },
      { "id": "1", "slots": 4 }
    ]
  }]
}
```

测试进程内可读取形如 `CTEST_RESOURCE_GROUP_COUNT`、`CTEST_RESOURCE_GROUP_0_GPUS` 的环境变量，得知自己被分配到了哪些 GPU id。资源组与资源锁的对比：

| 维度 | `RESOURCE_LOCK` | `RESOURCE_GROUPS` |
| :--- | :--- | :--- |
| 表达能力 | 仅"互斥同名锁" | 可量化资源种类、数量、slot |
| 是否需规格文件 | 否 | 是（`--resource-spec-file`） |
| 是否告知测试拿到啥 | 否 | 是（注入环境变量） |
| 典型场景 | 端口、临时文件互斥 | 多 GPU 测试编排 |

---

## 12.4 ctest 命令行

测试在构建完成后运行。最简单的两步：

```bash
cmake --build build            # 先构建（含测试可执行程序）
ctest --test-dir build         # 进入 build 运行全部测试
```

> 在 build 目录里直接敲 `ctest` 也行；`--test-dir build` 让你无需 `cd`。

下表汇总常用 `ctest` 选项。

| 选项 | 作用 |
| :--- | :--- |
| `-R <regex>` | 仅运行**名字匹配**正则的测试（include by name） |
| `-E <regex>` | **排除**名字匹配正则的测试（exclude by name） |
| `-L <regex>` | 仅运行**标签匹配**正则的测试；`-LE` 为按标签排除 |
| `-j <N>` / `--parallel <N>` | 并行运行，最多同时使用 N 个处理器 |
| `--output-on-failure` | 仅当测试失败时打印其完整输出（最常用） |
| `-V` / `--verbose` | 详细输出（打印每条测试的命令与输出） |
| `-VV` / `--extra-verbose` | 更详细，连内部调度细节也打印 |
| `--rerun-failed` | 只重跑上次失败的测试（覆盖 `-R/-E/-L/-LE/-I`） |
| `--repeat <mode>:<n>` | 按模式重复：`until-pass` / `until-fail` / `after-timeout` |
| `--repeat-until-fail <n>` | 旧式写法，等价 `--repeat until-fail:<n>` |
| `--timeout <s>` | 为没有自带 `TIMEOUT` 属性的测试设置全局超时（秒） |
| `-C <config>` | 多配置生成器下选择配置（Debug/Release 等） |
| `--test-dir <dir>` | 指定要运行测试的构建目录 |
| `--schedule-random` | 随机化测试运行顺序（暴露隐藏的顺序依赖） |
| `-N` / `--show-only` | 只**列出**将要运行的测试，不实际运行 |
| `--output-junit <file>` | 将结果写为 JUnit XML（CI 集成常用） |

### 12.4.1 选择运行哪些测试

**`-R <regex>`**：按测试名做正则**包含**匹配。

```bash
ctest -R '^unit_'       # 跑所有名字以 unit_ 开头的测试
ctest -R Math           # 跑名字含 Math 的测试
```

**`-E <regex>`**：按名正则**排除**。

```bash
ctest -E 'slow|network' # 跳过名字含 slow 或 network 的测试
```

**`-L <regex>` / `-LE <regex>`**：按 `LABELS` 属性正则匹配/排除（见 §12.3.3）。`-R/-E` 与 `-L/-LE` 可叠加，结果取交集。

```bash
ctest -L unit -E flaky  # 标签含 unit、但名字不含 flaky
```

**`-N`**：只列出会被选中的测试，不运行。改过滤条件时用来"预演"非常有用。

```bash
ctest -N -L integration
```

### 12.4.2 并行与输出

**`-j <N>`**：并行运行。`N` 是处理器预算，CTest 结合各测试的 `PROCESSORS`、`RUN_SERIAL`、`RESOURCE_LOCK` 调度。一般设为机器核数：

```bash
ctest -j$(nproc)              # Linux
ctest -j8 --output-on-failure
```

**`--output-on-failure`**：默认情况下 CTest 只在汇总里显示 `Passed/Failed`，不打印测试输出。加上此选项后，**失败**的测试会连同其完整 stdout/stderr 一起打印——这是排查失败的第一选择，建议在 CI 中常开。等价的环境变量是 `CTEST_OUTPUT_ON_FAILURE=1`。

**`-V` / `-VV`**：无条件提升啰嗦程度。`-V` 打印每条测试的命令行与输出；`-VV` 额外打印 CTest 内部细节。日常排查多用 `--output-on-failure`，定位"测试到底跑了什么命令"时才上 `-V`。

### 12.4.3 重跑与重复

**`--rerun-failed`**：只运行**上一次运行中失败**的那些测试。它会**覆盖**所有选择类选项（`-R/-E/-L/-LE/-I`）。修 bug → 重跑失败 → 修 → 再重跑，迭代极快。

```bash
ctest --output-on-failure        # 第一次全跑，有 3 个失败
# ...改代码、重新构建...
ctest --rerun-failed --output-on-failure   # 只重跑那 3 个
```

**`--repeat <mode>:<n>`**：按模式重复运行，用于对抗或捕捉**偶发性（flaky）**问题。三种模式（CMake 3.17 引入，4.3.4 沿用）：

- **`until-pass:<n>`**：每条测试最多重试 `n` 次，**任一次通过即算通过**。用于容忍偶发失败（比如偶发的网络抖动）。
- **`until-fail:<n>`**：要求每条测试连续运行 `n` 次**全部通过**才算通过，**任一次失败即整体失败**。用于**主动暴露**偶发失败——反复跑直到它露馅。
- **`after-timeout:<n>`**：每条测试最多重试 `n` 次，但**仅当上次因超时而失败时才重试**（其他原因的失败不重试）。用于在繁忙的 CI 机器上容忍偶发超时。

```bash
ctest --repeat until-pass:3      # 偶发失败容忍 3 次
ctest --repeat until-fail:50     # 反复跑 50 遍找隐藏的不稳定测试
ctest --repeat after-timeout:2   # 只对超时的测试再给 2 次机会
```

> 旧式 `--repeat-until-fail <n>` 完全等价于 `--repeat until-fail:<n>`，新代码用统一的 `--repeat` 即可。

### 12.4.4 超时、配置与目录

**`--timeout <s>`**：为**没有**自身 `TIMEOUT` 属性的测试设定全局超时秒数。测试自带的 `TIMEOUT` 属性优先级更高。

**`-C <config>`**：多配置生成器（VS / Xcode / Ninja Multi-Config）下选择要测试的配置。单配置生成器（普通 Ninja / Makefiles）忽略它。

```bash
ctest -C Release --output-on-failure
```

**`--test-dir <dir>`**：指定包含 `CTestTestfile.cmake` 的构建目录，免去 `cd`。

```bash
ctest --test-dir build/ -C Debug -j4
```

### 12.4.5 顺序随机化

**`--schedule-random`**：随机化测试执行顺序。测试本应彼此独立；若打乱顺序后开始有测试失败，往往说明它们之间存在**隐藏的顺序依赖**（例如前一个测试残留了文件或全局状态）。是发现"脏测试"的利器。

```bash
ctest --schedule-random --output-on-failure
```

---

## 12.5 与测试框架集成

手写 `add_test()` 适合少量黑盒测试。当用 GoogleTest / Catch2 写了成百上千个 `TEST` 时，逐个登记不现实。这些框架配套的 CMake 模块能**自动发现**每个测试并登记为独立的 CTest 用例，从而获得 CTest 的全部能力（并行、过滤、超时、单测重跑）。

### 12.5.1 GoogleTest：`gtest_discover_tests` 与 `gtest_add_tests`

CMake 自带 `GoogleTest` 模块（`include(GoogleTest)`），提供两个命令：

**`gtest_discover_tests(<target> ...)`（推荐）**：在**构建后或测试前**运行编译好的测试可执行程序、以 `--gtest_list_tests` 列出其全部测试（含参数化测试的每个实例），把**每一个 `TEST` 注册成一条独立的 CTest 用例**。优点：

- 自动覆盖参数化/类型化测试的所有实例；
- 测试增删后**无需重新运行 CMake**（发现发生在构建/测试时）。

签名（4.3.4）：

```cmake
gtest_discover_tests(target
                     [EXTRA_ARGS args...]
                     [WORKING_DIRECTORY dir]
                     [TEST_PREFIX prefix]
                     [TEST_SUFFIX suffix]
                     [TEST_FILTER expr]
                     [NO_PRETTY_TYPES] [NO_PRETTY_VALUES]
                     [PROPERTIES name1 value1...]
                     [TEST_LIST var]
                     [DISCOVERY_TIMEOUT seconds]
                     [XML_OUTPUT_DIR dir]
                     [DISCOVERY_MODE <POST_BUILD|PRE_TEST>]
                     [DISCOVERY_EXTRA_ARGS args...])
```

常用关键字：

- `PROPERTIES name value...`：把测试属性一次性赋给发现出来的整组测试。
- `TEST_PREFIX` / `TEST_SUFFIX`：给生成的 CTest 用例名加前/后缀，避免多个可执行程序间重名。
- `DISCOVERY_MODE POST_BUILD|PRE_TEST`：`POST_BUILD`（默认）在构建后立即发现；**交叉编译时**目标机和构建机不同，应改用 `PRE_TEST`，把发现推迟到运行测试之前（需正确设置 `CROSSCOMPILING_EMULATOR`）。
- `DISCOVERY_TIMEOUT`：发现阶段（列举测试）本身的超时。

**`gtest_add_tests(TARGET <target> ...)`（备选）**：在 **CMake 配置时**扫描源码里的 `TEST()` / `TEST_F()` 宏来登记测试。优点是测试在 CMake 阶段即可见、设属性方便、对交叉编译友好；缺点是**无法发现参数化测试的具体实例**，且增删测试需重跑 CMake。

```cmake
gtest_add_tests(TARGET my_tests
                SOURCES ${test_sources}
                TEST_LIST added_tests)
```

二者取舍：**默认首选 `gtest_discover_tests`**；仅在交叉编译且无法配置模拟器、或确实需要在配置期拿到测试列表时考虑 `gtest_add_tests`。

### 12.5.2 完整可运行示例（FetchContent 拉取 GoogleTest）

下面是一个端到端最小工程：用 `FetchContent` 拉取 GoogleTest → 写一个被测函数与测试 → 自动发现 → `ctest` 运行。

目录结构：

```text
gtest-demo/
├── CMakeLists.txt
├── src/
│   ├── math.h
│   └── math.cpp
└── tests/
    └── test_math.cpp
```

`src/math.h`：

```cpp
#pragma once
int add(int a, int b);
```

`src/math.cpp`：

```cpp
#include "math.h"
int add(int a, int b) { return a + b; }
```

`tests/test_math.cpp`：

```cpp
#include <gtest/gtest.h>
#include "math.h"

TEST(MathTest, AddsPositive) { EXPECT_EQ(add(2, 3), 5); }
TEST(MathTest, AddsNegative) { EXPECT_EQ(add(-1, -1), -2); }
```

顶层 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 4.3)
project(GTestDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 被测库 ---
add_library(mymath src/math.cpp)
target_include_directories(mymath PUBLIC src)

# --- 启用测试 ---
enable_testing()

# --- 用 FetchContent 拉取 GoogleTest ---
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2        # 固定一个可复现的发行 tag
)
# Windows: 强制 gtest 使用与主项目一致的运行时库
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- 测试可执行程序 ---
add_executable(test_math tests/test_math.cpp)
target_link_libraries(test_math
  PRIVATE mymath GTest::gtest_main)   # gtest_main 提供 main()

# --- 自动发现每个 TEST 为独立 CTest 用例 ---
include(GoogleTest)
gtest_discover_tests(test_math)
```

构建并运行：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

输出会显示 `MathTest.AddsPositive`、`MathTest.AddsNegative` **两条独立用例**（而非一个 `test_math` 整体），证明发现成功。此时即可 `ctest --test-dir build -R AddsPositive` 单独重跑某一个。

### 12.5.3 Catch2：`catch_discover_tests`

Catch2 v3 同样提供发现机制。拉取 Catch2 后 `include(Catch)`，用 `catch_discover_tests(<target>)` 把每个 `TEST_CASE` 注册为独立 CTest 用例，用法与 GoogleTest 高度一致：

```cmake
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(catch_tests tests/test_catch.cpp)
target_link_libraries(catch_tests PRIVATE Catch2::Catch2WithMain)

# Catch2 把它的 CMake 模块放进了构建目录，需要加到模块路径
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(catch_tests)
```

```cpp
// tests/test_catch.cpp
#include <catch2/catch_test_macros.hpp>
#include "math.h"
TEST_CASE("add works", "[math]") { REQUIRE(add(2, 3) == 5); }
```

doctest 也提供类似的 `doctest_discover_tests`，思路相同，不再赘述。

---

## 12.6 fixtures 实战

`DEPENDS` 只能排序，不能表达"建立环境 → 跑测试 → 拆除环境"且"建立失败则测试不跑"的完整生命周期。这正是 **fixtures** 要解决的问题，依赖三个属性：

- **`FIXTURES_SETUP <fixture>`**：标记某测试是 fixture 的**建立**步骤。
- **`FIXTURES_CLEANUP <fixture>`**：标记某测试是 fixture 的**清理**步骤；即使中途有测试失败，清理仍会运行。
- **`FIXTURES_REQUIRED <fixture>`**：标记某测试**需要**该 fixture。CTest 会保证：
  1. 在它之前自动运行对应的 `FIXTURES_SETUP` 测试；
  2. 在它之后（且整组都跑完后）自动运行对应的 `FIXTURES_CLEANUP` 测试；
  3. **若 setup 失败，所有 require 它的测试被标记为 Not Run**（这是与 `DEPENDS` 的关键区别）。

而且：用 `-R` 只选中了某个 require fixture 的测试时，CTest 会**自动把它依赖的 setup/cleanup 也拉进来运行**，无需手动指定。

下面演示"建库 → 跑若干数据库测试 → 拆库"的编排：

```cmake
enable_testing()

# 各步骤对应的可执行程序/脚本
add_test(NAME db_setup   COMMAND ${CMAKE_COMMAND} -P scripts/init_db.cmake)
add_test(NAME db_cleanup COMMAND ${CMAKE_COMMAND} -P scripts/drop_db.cmake)
add_test(NAME db_test_users   COMMAND db_test --suite users)
add_test(NAME db_test_orders  COMMAND db_test --suite orders)

# 声明 setup / cleanup 属于名为 "DB" 的 fixture
set_tests_properties(db_setup   PROPERTIES FIXTURES_SETUP   DB)
set_tests_properties(db_cleanup PROPERTIES FIXTURES_CLEANUP DB)

# 两个测试都需要 DB fixture
set_tests_properties(db_test_users db_test_orders
                     PROPERTIES FIXTURES_REQUIRED DB)

# 这些测试都会读写同一个库，加资源锁避免它们并发互相干扰
set_tests_properties(db_setup db_cleanup db_test_users db_test_orders
                     PROPERTIES RESOURCE_LOCK DbAccess)
```

无论你怎样筛选运行，CTest 都会保证顺序为：`db_setup` → （`db_test_users`、`db_test_orders` 任意顺序）→ `db_cleanup`。

```bash
ctest -R db_test_users --output-on-failure
# CTest 自动补齐：先 db_setup，再 db_test_users，最后 db_cleanup
```

要点回顾：

- 一个测试可以同时是多个 fixture 的 setup/cleanup（列表传值，如 `"DB;Cache"`）。
- `FIXTURES_CLEANUP` 的清理具有"尽力而为"的语义：即使中间测试失败，CTest 仍会运行清理，避免环境泄漏。
- fixture 名只是字符串标识，自行约定即可（如 `DB`、`Server`、`TempDir`）。

---

## 12.7 CTest 脚本与 Dashboard

CTest 不仅能在本地跑测试，还能作为**持续集成/夜间构建**的驱动，自动完成"更新源码 → 配置 → 构建 → 测试 → 提交结果到仪表盘"，并把结果汇总到 **CDash**（Kitware 的开源 Web 仪表盘）。

### 12.7.1 Dashboard 三种模式

CTest 把一次完整的 dashboard 流程切成若干步骤（Start/Update/Configure/Build/Test/Coverage/MemCheck/Submit），并预设三类常见组合：

| 模式 | 触发方式 | 典型用途 |
| :--- | :--- | :--- |
| **Experimental** | 开发者手动触发 | 本地随时跑一次并（可选）上报，验证改动 |
| **Nightly** | 定时（每晚）触发 | 以"昨夜某固定时刻"的源码快照为基线，全量构建+测试 |
| **Continuous** | 检测到提交即触发 | 持续监视仓库，有新提交就增量构建+测试 |

命令行可直接用 `-D <model>` 触发一次内置流程（需要工程提供 `CTestConfig.cmake`）：

```bash
ctest -D Experimental            # 配置+构建+测试+提交，一条龙
ctest -D ExperimentalTest        # 只做测试这一步
ctest -D NightlyMemoryCheck      # 夜间内存检查
```

### 12.7.2 `CTestConfig.cmake`：仪表盘坐标

要把结果提交到 CDash，工程根目录需放一个 `CTestConfig.cmake`，声明项目名与 CDash 服务器地址。它会被 `include(CTest)` 自动加载：

```cmake
set(CTEST_PROJECT_NAME "MyApp")
set(CTEST_NIGHTLY_START_TIME "01:00:00 UTC")

set(CTEST_DROP_METHOD "https")
set(CTEST_DROP_SITE "my.cdash.org")
set(CTEST_DROP_LOCATION "/submit.php?project=MyApp")
set(CTEST_DROP_SITE_CDASH TRUE)
```

### 12.7.3 `ctest -S script.cmake`：脚本驱动模式

最灵活的方式是写一个 **CTest 脚本**（`-S` 选项），在其中用 `ctest_start()` / `ctest_configure()` / `ctest_build()` / `ctest_test()` / `ctest_submit()` 等命令精确编排整个流程。脚本是普通的 CMake 语言文件，但运行在"CTest 脚本模式"下，可使用一组 `ctest_*` 命令与 `CTEST_*` 变量。

一个典型的 CI 脚本 `dashboard.cmake`：

```cmake
# 源码与构建目录
set(CTEST_SOURCE_DIRECTORY "$ENV{CI_PROJECT_DIR}")
set(CTEST_BINARY_DIRECTORY "$ENV{CI_PROJECT_DIR}/build")

set(CTEST_CMAKE_GENERATOR "Ninja")
set(CTEST_BUILD_CONFIGURATION "Release")
set(CTEST_SITE "ci-runner-01")            # 提交到 CDash 时显示的机器名
set(CTEST_BUILD_NAME "linux-gcc-release") # 构建标识

ctest_start(Continuous)                   # 选择 dashboard 模型
ctest_configure()
ctest_build()
ctest_test(PARALLEL_LEVEL 8 RETURN_VALUE test_result
           REPEAT until-pass:2)           # 脚本里也能用 REPEAT/PARALLEL_LEVEL
ctest_submit()                            # 提交结果到 CDash（需 CTestConfig.cmake）

if(NOT test_result EQUAL 0)
  message(FATAL_ERROR "测试存在失败")
endif()
```

运行：

```bash
ctest -S dashboard.cmake -V
```

`ctest_test()` 的关键参数与命令行选项一一对应：`INCLUDE`/`EXCLUDE`（≈ `-R`/`-E`）、`INCLUDE_LABEL`/`EXCLUDE_LABEL`（≈ `-L`/`-LE`）、`PARALLEL_LEVEL`（≈ `-j`）、`REPEAT <mode>:<n>`（≈ `--repeat`）、`SCHEDULE_RANDOM`、`OUTPUT_JUNIT` 等。脚本模式是把 CTest 嵌入企业 CI 流水线的标准做法。

---

## 12.8 内存检查与覆盖率

CTest 内置对**动态分析工具**的调度支持：`ctest -T <action>` 触发"测试+附加分析"的组合步骤。

### 12.8.1 内存检查 `ctest -T memcheck`

`memcheck` 步骤会在每个测试外面包一层内存检查工具（默认 Valgrind，也支持 AddressSanitizer、Dr. Memory、Purify 等），运行测试并收集内存错误（泄漏、越界、未初始化读等）报告。

需要先告诉 CTest 用哪个工具及其参数，通常在 CTest 脚本或缓存变量中设置：

- **`CTEST_MEMORYCHECK_COMMAND`**：内存检查工具可执行文件路径（如 `/usr/bin/valgrind`）。
- **`CTEST_MEMORYCHECK_COMMAND_OPTIONS`**：传给工具的额外参数（如 `--leak-check=full --error-exitcode=1`）。
- **`CTEST_MEMORYCHECK_SUPPRESSIONS_FILE`**：抑制文件，屏蔽已知的第三方误报。

命令行触发：

```bash
# 配置阶段指明 valgrind（也可写进 CTestConfig.cmake / 脚本）
cmake -S . -B build -DMEMORYCHECK_COMMAND=$(which valgrind)
cmake --build build
ctest --test-dir build -T memcheck --output-on-failure
```

或在 CTest 脚本里：

```cmake
set(CTEST_MEMORYCHECK_COMMAND "/usr/bin/valgrind")
set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")
ctest_start(Experimental)
ctest_test()
ctest_memcheck()           # 对应 -T memcheck，收集内存缺陷
ctest_submit()             # 缺陷报告随之上报 CDash
```

`ctest_memcheck()` 还能用 `DEFECT_COUNT <var>` 取回缺陷计数，便于在脚本中据此让 CI 失败。

### 12.8.2 覆盖率 `ctest -T coverage`

`coverage` 步骤收集代码覆盖率数据并汇总（GCC/Clang 用 gcov 系，配合 `--coverage` 编译选项）。流程通常是：

1. **编译期**加覆盖率插桩选项（GCC/Clang）：

```cmake
add_library(mymath src/math.cpp)
target_compile_options(mymath PRIVATE --coverage)
target_link_options(mymath    PRIVATE --coverage)
```

2. **运行测试**生成 `.gcda` 数据。
3. **`ctest -T coverage`** 调用 gcov 解析并汇总：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build                # 先跑测试，产生 .gcda
ctest --test-dir build -T coverage    # 收集并汇总覆盖率
```

相关变量：`CTEST_COVERAGE_COMMAND`（gcov 可执行路径）、`CTEST_COVERAGE_EXTRA_FLAGS`。在 CTest 脚本中对应 `ctest_coverage()`，其结果同样可 `ctest_submit()` 到 CDash，在网页上以行级覆盖热图展示。

### 12.8.3 `include(CTest)` 与 `BUILD_TESTING`

前文多次提到 `include(CTest)`，这里集中说明。在顶层 `CMakeLists.txt` 中 `include(CTest)` 会：

1. **自动定义缓存选项 `BUILD_TESTING`（默认 `ON`）**；
2. 仅当 `BUILD_TESTING` 为真时，**自动调用 `enable_testing()`**；
3. 提供前述全套 dashboard 相关变量与 `-T` 动作（memcheck / coverage）的脚手架，并自动加载 `CTestConfig.cmake`。

因此惯用法是用 `BUILD_TESTING` 把整套测试代码包起来，让下游可一键关闭测试构建：

```cmake
cmake_minimum_required(VERSION 4.3)
project(MyApp LANGUAGES CXX)

include(CTest)                 # 定义 BUILD_TESTING（默认 ON）并调用 enable_testing()

add_subdirectory(src)

if(BUILD_TESTING)              # 仅在需要时才配置测试目标
  add_subdirectory(tests)
endif()
```

```bash
cmake -S . -B build -DBUILD_TESTING=OFF   # 作为依赖被集成时，跳过测试，加速构建
```

> 选型小结：**只想跑测试 → `enable_testing()` 足矣；想要 `BUILD_TESTING` 开关、内存/覆盖率、CDash 上报 → 用 `include(CTest)`。** 两者不要同时写（`include(CTest)` 已含 `enable_testing()`）。

---

## 本章小结

- **CTest 是调度器，不是断言库**：它负责"运行哪些命令、怎么并行、超时多久、失败怎么重跑、结果如何汇总上报"，断言交给 GoogleTest / Catch2 / doctest。
- **两条核心命令**：顶层 `enable_testing()`（或更全能的 `include(CTest)`，附带 `BUILD_TESTING`）开启测试；`add_test(NAME ... COMMAND ...)` 登记测试，命令用**目标名**或 `$<TARGET_FILE:t>` 引用被测程序，杜绝硬编码路径。
- **测试属性**用 `set_tests_properties` 设置：`WILL_FAIL`/`PASS_REGULAR_EXPRESSION` 控判定，`TIMEOUT`/`DISABLED`/`SKIP_RETURN_CODE` 控生命周期，`LABELS`/`DEPENDS` 控分组排序，`ENVIRONMENT`/`ENVIRONMENT_MODIFICATION`/`WORKING_DIRECTORY` 控运行环境，`RUN_SERIAL`/`PROCESSORS`/`RESOURCE_LOCK`/`RESOURCE_GROUPS` 控并行与资源。
- **ctest 命令行**：`-R/-E` 按名筛选、`-L/-LE` 按标签筛选、`-j` 并行、`--output-on-failure` 必备、`--rerun-failed` 快速迭代、`--repeat until-pass|until-fail|after-timeout` 对付偶发问题、`--schedule-random` 揪出顺序依赖、`-N` 预演。
- **框架集成**：`include(GoogleTest)` + `gtest_discover_tests()` 把每个 `TEST` 自动注册成独立 CTest 用例（交叉编译用 `DISCOVERY_MODE PRE_TEST`）；`gtest_add_tests` 在配置期扫描源码登记；Catch2 对应 `catch_discover_tests`。配合 `FetchContent` 可零依赖拉取框架。
- **fixtures**：`FIXTURES_SETUP`/`FIXTURES_REQUIRED`/`FIXTURES_CLEANUP` 编排"建库→测试→拆库"，setup 失败会让相关测试 Not Run，清理尽力而为——这是 `DEPENDS` 做不到的。
- **脚本与 Dashboard**：`ctest -S script.cmake` 用 `ctest_start/configure/build/test/submit` 驱动 CI，`CTestConfig.cmake` 指明 CDash 坐标，Experimental/Nightly/Continuous 三模型。
- **动态分析**：`ctest -T memcheck`（Valgrind，配 `CTEST_MEMORYCHECK_COMMAND`）查内存错误，`ctest -T coverage`（gcov，配 `--coverage` 编译选项）收覆盖率，结果均可上报 CDash。

> ⬅️ [[42.Cmake/11 - 安装、导出与打包.md|上一章]] ｜ ➡️ [[42.Cmake/13 - 工具链与交叉编译.md|第 13 章]]
>
> [[00 - CMake 完整技术教程 - 总索引|总索引]]
