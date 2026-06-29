---
title: CMake 语言基础
tags: [cmake, language, reference]
chapter: "02"
cmake_version: 4.3.4
---

# 第 02 章 · CMake 语言基础

> 本章目标：彻底讲清 **CMake Language（CMake 语言）本身的语法**——它是怎样被一行一行解析、求值的；参数有哪几种写法、什么时候要加引号；变量分几类、怎么解引用；条件真假怎么判定；控制流、函数/宏、作用域、脚本复用的全部规则与边界情况。
>
> 基准版本 **CMake 4.3.4（2026-06）**；除非显式标注 `(3.x+)`，否则示例在现代 CMake（3.20 及以上）通用。CMake 语言的语法自 3.x 以来高度稳定，本章给出的是「随便哪个现代版本都成立」的核心规则。

读完本章你应当能够：脱离 IDE 看懂任意 `CMakeLists.txt` 的每一行；准确预判一段代码会被展开成什么；不再被「为什么这里的字符串没展开 / 列表被拆成了多个参数 / 变量取出来是空的」这类问题困住。

---

## 2.1 Listfile 与基本结构

CMake 处理的源文件统称 **listfile（清单文件）**。它有两种身份：

- **`CMakeLists.txt`**：项目的构建描述文件。每个参与构建的目录里放一个，`cmake` 从顶层目录的 `CMakeLists.txt` 开始读，遇到 `add_subdirectory()` 再去读子目录的 `CMakeLists.txt`。文件名固定、大小写敏感（在大小写敏感的文件系统上）。
- **`*.cmake` 脚本/模块**：可被 `include()` 引入到当前作用域执行，或被 `cmake -P xxx.cmake` 当作独立脚本运行，或作为 `find_package()` 的模块。命名自由，习惯用 `.cmake` 后缀。

两者用的是**同一种语言**，区别只是「谁来加载、在什么上下文执行」。

### 2.1.1 命令式语言，逐行求值

CMake 语言是一门**命令式（imperative）解释型脚本语言**，不是声明式配置。它的执行模型极其简单，记住这三句话就抓住了本质：

1. **整个文件就是一串「命令调用」**，没有别的东西。注释、空白之外，每一个有意义的 token 都属于某个命令调用。
2. **解释器从上到下、一行一行（严格说是一条命令一条命令）地求值**，先出现的命令先执行，后面的命令能看到前面命令产生的副作用（比如设过的变量、定义过的目标）。
3. **没有「返回值」概念**。命令不返回值，它通过**副作用**工作——修改变量、定义目标、向某个输出变量写结果（约定把结果变量名作为参数传进去，例如 `string(LENGTH "abc" out)` 把长度写到 `out`）。

```cmake
# 一个最小 listfile：三条命令，自上而下执行
cmake_minimum_required(VERSION 3.20)   # 命令 1：声明最低版本
project(Hello LANGUAGES CXX)           # 命令 2：定义工程
message(STATUS "项目名是 ${PROJECT_NAME}")  # 命令 3：打印（能看到命令 2 设的变量）
```

> ⚠️ 「逐行求值」意味着**顺序极其重要**。在 `project()` 之前引用编译器相关变量、在 `add_executable()` 之前调 `target_link_libraries()`，都会因为「那时候东西还不存在」而失败。这是初学者最常踩的坑，远多于语法错误。

### 2.1.2 一次 cmake 运行的两个阶段（先建立直觉）

虽然本章只讲语言语法，但有个背景必须先知道，否则后面理解「生成器表达式为什么不能用 `if` 判断」之类会很别扭：

- **配置阶段（Configure）**：CMake 解释器执行你写的 listfile，所有命令、`if`、`foreach`、`function` 都在这个阶段跑，产生内部的目标模型。**本章讲的一切语法都发生在这一阶段。**
- **生成阶段（Generate）**：CMake 把内部模型翻译成具体构建系统（Makefile / Ninja / VS 工程）。此阶段不再执行你的脚本逻辑，只有「生成器表达式 `$<...>`」会在这里求值。

记住：**CMake 语言 = 配置阶段的脚本**。生成器表达式 `$<...>` 是另一套东西（第 12 章详解），不要和本章的 `${}`、`$ENV{}` 混淆。

---

## 2.2 注释

CMake 有两种注释，都以 `#` 起头。

### 2.2.1 行注释 `#`

从 `#` 到行尾被忽略。最常用。

```cmake
# 这是一整行注释
set(SRC main.cpp)   # 行尾注释：从 # 到行尾都被忽略
```

### 2.2.2 块注释（bracket comment）`#[[ ... ]]`

紧跟在 `#` 后面写一个**方括号串（bracket）**，即 `[` + 任意个 `=` + `[`，直到匹配的 `]` + 同样个数的 `=` + `]` 为止，中间所有内容（含换行）都是注释。它是「行注释 `#` + 方括号参数语法」的组合，所以叫 bracket comment。

```cmake
#[[ 这是块注释，可以
跨越多行，
中间的 # 不会被特殊对待。 ]]

#[==[
当注释内容里本身含有 ]] 时，
用更多的等号把外层括起来，例如这里用 [==[ ... ]==]，
就不会被内部的 ]] 提前结束。
]==]
```

**规则要点**：

- **`[[` 必须紧跟在 `#` 之后**（`#[[`），中间不能有空格。`# [[` 会被当成普通行注释，`[[` 成了注释文字。
- **等号个数必须左右一致**：`#[=[ ... ]=]`、`#[==[ ... ]==]`。靠加等号来避开内容里出现的闭合括号，这点和后面「bracket 参数」完全同源（见 2.4.1）。
- 块注释可以出现在命令参数之间，但很少这么用，可读性差。

---

## 2.3 命令调用语法

CMake 文件里**唯一的「语句」就是命令调用（command invocation）**，形式固定：

```
command_name(arg1 arg2 arg3 ...)
```

```cmake
add_executable(app main.cpp util.cpp)
#  └命令名┘ └────── 参数列表 ──────┘
```

### 2.3.1 命令名：大小写不敏感，约定小写

**命令名大小写不敏感**：`add_executable`、`ADD_EXECUTABLE`、`Add_Executable` 完全等价，解释器会忽略大小写匹配命令。

```cmake
PROJECT(Demo)        # 合法
Project(Demo)        # 合法
project(Demo)        # 合法（推荐）
```

> 📌 **约定**：现代 CMake 社区一致约定**命令名一律小写**（`project`、`set`、`if`、`target_link_libraries`）。老教程里常见的全大写 `PROJECT()`、`SET()` 是 2.x 时代的遗风，新代码不要再用。**大小写不敏感只针对命令名**——变量名、关键字参数（如 `PUBLIC`、`STATIC`、`VERSION`）是**大小写敏感**的，必须照写。

### 2.3.2 左括号必须紧跟命令名

命令名和 `(` 之间**不允许有空白**（这是少数硬性词法规则之一）：

```cmake
message("ok")    # 正确
message ("ok")   # 错误：命令名与 ( 之间有空格，解析失败
```

### 2.3.3 参数分隔：靠空白，不是逗号

**参数之间用空白（空格、制表符、换行）分隔，绝对不是逗号！** 这是从别的语言转过来最容易写错的一点。

```cmake
# 正确：三个参数，用空格分隔
add_executable(app main.cpp util.cpp)

# 错误：逗号会被当成参数文字的一部分，
# 实际传入的参数变成了 "main.cpp," 这种带逗号的怪东西
add_executable(app main.cpp, util.cpp)
```

参数可以跨多行书写，换行同样只是空白分隔符，便于排版：

```cmake
target_link_libraries(app
    PRIVATE
        fmt::fmt
        spdlog::spdlog
)
```

### 2.3.4 命令的几个来源

调用前命令必须「已存在」。命令有三个来源：

- **内置命令（scripting/project commands）**：`set`、`if`、`message`、`add_library`…… 解释器自带。
- **由模块提供的命令**：`include(GNUInstallDirs)`、`find_package(...)` 等会引入额外命令/宏。
- **自定义命令**：你用 `function()` / `macro()` 定义的（见 2.9）。定义之后才能在后面调用——又是「逐行求值」的体现。

---

## 2.4 参数类型（最关键、最易错的一节）

每个参数在词法层面属于三种之一：**bracket argument（方括号参数）**、**quoted argument（引号参数）**、**unquoted argument（无引号参数）**。它们决定了「转义和 `${}` 变量展开是否生效」「内容会不会被拆成多个参数」。这是整章最该吃透的部分。

| 参数类型 | 写法 | `${var}` 展开 | `\` 转义 | `;` 是否拆分列表 | 典型用途 |
|---|---|---|---|---|---|
| **bracket** | `[[ ... ]]` / `[=[ ... ]=]` | ❌ 不展开 | ❌ 不处理 | ❌ 整体一个参数 | 正则、嵌入脚本、含特殊字符的原始文本 |
| **quoted** | `"..."` | ✅ 展开 | ✅ 处理 | ❌ 整体一个参数 | 路径、含空格的字符串、消息文本 |
| **unquoted** | `xxx`（裸写） | ✅ 展开 | ✅ 处理 | ✅ **按 `;` 拆成多个参数** | 简单 token、列表传参 |

### 2.4.1 Bracket argument 方括号参数 `[[ ]]` / `[=[ ]=]`

把内容**原封不动**当作单个参数，**不做任何变量展开、不处理任何转义**。语法和块注释同源：`[` + N 个 `=` + `[` 开始，`]` + 同样 N 个 `=` + `]` 结束。

```cmake
# 不展开 ${PATH}，反斜杠原样保留，整体是一个参数
message([[原始文本：${PATH} 和 C:\temp\new 都不会被处理]])

# 内容里含有 ]] 时，加等号避免提前闭合
message([=[ 这里可以安全地写 ]] 而不会被截断 ]=])
```

**何时用**：写正则表达式（避免反斜杠被吃掉）、嵌入一段其它语言的脚本、`configure_file` 里塞原始模板内容、任何「我就要字面量、别给我动」的场合。

> 小细节：紧跟在开括号后的**第一个换行符会被忽略**，方便你把内容另起一行写得整齐（上面 `message([=[` 后换行不会进入文本）。

### 2.4.2 Quoted argument 引号参数 `"..."`

用双引号包起来，**永远是单个参数**（即使里面有空格或 `;`），并且**会展开 `${}` 变量、会处理 `\` 转义**。

```cmake
set(NAME "张 三")                 # 含空格，必须用引号，否则会被拆成两个参数
message("你好，${NAME}！")          # ${NAME} 被展开 → 你好，张 三！
message("制表\t换行\n反斜杠\\")     # 转义生效：\t \n \\ 分别是制表/换行/反斜杠
```

**转义序列**（在 quoted 和 unquoted 参数里都有效，bracket 里无效）：

| 转义 | 含义 |
|---|---|
| `\\` | 反斜杠字面量 `\` |
| `\"` | 双引号字面量 `"`（在引号参数里用来嵌入 `"`） |
| `\n` | 换行 |
| `\t` | 制表符 |
| `\r` | 回车 |
| `\;` | **分号字面量**——一个不触发列表拆分的 `;`（极其重要，见 2.6） |
| `\#` `\(` `\)` `\$` `\@` `\^` 等 | 对应的字面字符（把本来有特殊含义的字符转成普通字符） |
| 行尾 `\`（续行） | 在引号参数里，行尾反斜杠紧接换行表示**续行**，把长字符串拆成多行书写，结果不含换行 |

```cmake
# 续行示例：结果是一整行，没有换行符
message("这是很长的一句话，\
为了源码好看而折行，\
但展开后是连续的一行。")

# 用 \" 在引号字符串里嵌入双引号
message("他说：\"你好\"")          # 输出：他说："你好"
```

**何时必须用引号**：参数里含**空格**、含**分号但你不想让它拆成列表**、含 `#` `(` `)` 等特殊字符、或者你想明确「这是一个完整字符串」。**经验法则**：路径、人类可读文本、可能为空或含空格的变量取值，统统加引号——`"${SOME_PATH}"` 比裸写 `${SOME_PATH}` 安全得多。

### 2.4.3 Unquoted argument 无引号参数（裸写）

不加任何引号直接写。它**会展开 `${}`、会处理 `\` 转义**，但有个关键行为：

- **不能直接包含空白**（空白就是参数分隔符；要含空白得转义或加引号）；
- **不能直接包含 `(` `)` `#` `"` `\`**（除非转义）；
- **展开后若结果里含 `;`，会被拆成多个参数（列表展开）！** 这是 unquoted 和 quoted 最本质的区别。

```cmake
set(FILES a.cpp b.cpp c.cpp)   # FILES 的值其实是字符串 "a.cpp;b.cpp;c.cpp"

# unquoted：${FILES} 展开后含 ; → 拆成 3 个参数，正好喂给 add_executable
add_executable(app ${FILES})           # 等价于 add_executable(app a.cpp b.cpp c.cpp)

# quoted：${FILES} 展开后整体是 1 个参数 "a.cpp;b.cpp;c.cpp"
add_executable(app2 "${FILES}")        # 错误！把带分号的怪文件名当成单个源文件
```

上面这个对比把「引号 vs 无引号」的实战意义讲透了：

- **要把列表「摊开」成多个参数** → 用 **unquoted** `${LIST}`。
- **要把值当成「一个整体」（哪怕里面有分号/空格）** → 用 **quoted** `"${VALUE}"`。

> 还有一种历史遗留的 **unquoted legacy（旧式无引号）** 写法（如 `a"b c"d` 这种引号嵌在中间、或裸写的 `$(MAKEVAR)`），是为了兼容 CMake 2.x，**新代码一律不要写**，知道有这回事即可。

---

## 2.5 变量

变量是 CMake 脚本的命脉。先建立一个核心认知：**CMake 的变量值永远是字符串**（见 2.6），所谓「数字」「布尔」「列表」都是字符串的不同解读方式。

### 2.5.1 三类变量总览

CMake 有**三类语义不同的变量**，务必区分，它们的解引用语法都不一样：

| 类别 | 设置方式 | 解引用 | 存活范围 | 说明 |
|---|---|---|---|---|
| **普通变量 normal** | `set(X ...)` | `${X}` | 当前作用域（函数/目录），见 2.10 | 最常用，不持久化 |
| **缓存变量 CACHE** | `set(X ... CACHE TYPE "doc")` | `${X}` 或 `$CACHE{X}` | 全局、跨次运行持久化（存进 `CMakeCache.txt`） | 用户可配置项、`-D` 注入 |
| **环境变量 ENV** | `set(ENV{X} ...)` | `$ENV{X}` | 当前 cmake 进程内 | 读写 OS 环境变量 |

### 2.5.2 普通变量：`set()` 与 `unset()`

```cmake
set(<variable> <value>... [PARENT_SCOPE])      # 设置（或 PARENT_SCOPE 写到父作用域）
unset(<variable> [PARENT_SCOPE])               # 删除（使其「未定义」）
```

```cmake
set(GREETING "Hello")          # 单值
set(SOURCES a.cpp b.cpp c.cpp) # 多值 → 自动成为列表 "a.cpp;b.cpp;c.cpp"
set(EMPTY "")                  # 空字符串（注意：这与「未定义」不同，见 2.5.6）
unset(GREETING)               # 删除变量，之后 ${GREETING} 求值为空
```

- 传**多个值**时，CMake 用 `;` 把它们连起来，于是「设置多值」就等于「创建列表」。
- `set(X)`（不给任何值）等同于 `unset(X)`，把变量变为未定义。
- `PARENT_SCOPE` 把变量写到上一层作用域，详见 2.10。

### 2.5.3 解引用 `${VAR}` 与嵌套 `${${x}}`

`${VAR}` 取出变量的值。它可以**嵌套，从内向外（inside-out）逐层求值**：

```cmake
set(x VERSION)
set(VERSION "1.2.3")
message("${${x}}")             # 内层 ${x}→VERSION，外层 ${VERSION}→1.2.3，输出 1.2.3

# 拼接变量名（动态变量名很常见）
set(LANG CXX)
set(CMAKE_CXX_STANDARD 17)
message("C++ 标准是 ${CMAKE_${LANG}_STANDARD}")  # 拼出 CMAKE_CXX_STANDARD → 17
```

`${}` 在 **quoted 和 unquoted 参数里都会展开**；在 **bracket 参数里不展开**（见 2.4）。

### 2.5.4 缓存变量与 `$CACHE{}`

缓存变量持久化到构建目录的 `CMakeCache.txt`，可被命令行 `-D` 覆盖、被 `ccmake`/`cmake-gui` 编辑。它是「给用户调的旋钮」（细节见第 04 章）。

```cmake
# 形式：set(<var> <value> CACHE <TYPE> "<docstring>" [FORCE])
set(BUILD_TESTS ON CACHE BOOL "是否编译测试")
set(INSTALL_DIR "/opt/app" CACHE PATH "安装路径")
```

- **`<TYPE>`**：`BOOL` / `STRING` / `PATH` / `FILEPATH` / `INTERNAL`，影响 GUI 显示方式。
- **`$CACHE{X}`**：**只**读缓存条目的值，**绕过同名普通变量**。普通的 `${X}` 在普通变量与缓存变量同名时优先取普通变量，`$CACHE{X}` 则强制看缓存。

```cmake
set(FOO local)                       # 普通变量
set(FOO cached CACHE STRING "")      # 同名缓存变量（普通变量存在时不覆盖缓存语义）
message("${FOO}")                    # 取普通变量 → local
message("$CACHE{FOO}")               # 强制取缓存  → cached
```

> `$CACHE{}` 自 CMake 3.13 起可用，日常较少直接用，了解它能「穿透」普通变量遮蔽即可。

### 2.5.5 环境变量 `$ENV{}`

```cmake
message("PATH = $ENV{PATH}")          # 读 OS 环境变量
set(ENV{MY_FLAG} "1")                 # 写（仅影响当前 cmake 进程及其子进程）
unset(ENV{MY_FLAG})                   # 删除环境变量
```

- 环境变量改动**只在本次 cmake 运行内有效**，不会写回 shell。
- 读取未设置的环境变量得到空字符串。
- 慎用：依赖环境变量会降低构建可复现性，能用 cache 变量就别用 ENV。

### 2.5.6 未定义变量求值为空字符串

**这是 CMake 一个温柔但危险的设计：引用一个从未定义（或已 `unset`）的变量，不会报错，而是求值为空字符串 `""`。**

```cmake
message("[${NOT_DEFINED_AT_ALL}]")    # 输出 []，不报错
```

带来的后果与防御：

```cmake
# 危险：若 SRC 拼错成 SRCS（未定义），展开为空，命令静默接收 0 个源文件
add_executable(app ${SRC})

# 防御 1：用 if(DEFINED ...) 显式判断「定义过没有」
if(NOT DEFINED SRC)
    message(FATAL_ERROR "变量 SRC 未定义")
endif()

# 防御 2：区分「未定义」与「定义为空串」——二者不同！
set(E "")
if(DEFINED E)            # 真：E 定义过（值是空串）
    message("E 已定义但为空")
endif()
```

> 记牢：**`DEFINED X` 判断的是「X 这个变量存不存在」，与它的值是不是空、是不是 false 无关。** 把「未定义」和「值为假/空」分开想，能避免大量玄学 bug。

---

## 2.6 字符串与列表

### 2.6.1 一切皆字符串

CMake 没有独立的数值、布尔、数组类型——**所有变量值在内部都是字符串**。`set(N 42)` 里的 `N` 值是字符串 `"42"`，只是某些命令（`math()`、`if(... EQUAL ...)`）会把它**当作数字解读**；`if()` 会把某些字符串**当作布尔解读**（见 2.7）。理解这一点，很多「类型」困惑就消失了。

### 2.6.2 列表 = 用分号 `;` 分隔的字符串

**CMake 的「列表」不是一种数据结构，而是「内部含有 `;` 分隔符的普通字符串」的别名。**

```cmake
set(L a b c)            # L 的值就是字符串 "a;b;c"
message("${L}")         # 输出 a;b;c（引号参数，整体一个，分号原样显示）
foreach(x IN LISTS L)   # 但能被当列表遍历 → a, b, c
    message("- ${x}")
endforeach()
```

由此推出几条「列表的真相」：

- **空列表 = 空字符串**。长度为 0。
- **单元素列表 = 不含分号的字符串**。`set(L hello)` 既是字符串 `hello` 也是单元素列表。
- **列表里塞含分号的元素很麻烦**：因为分号本身就是分隔符。要表示「字面分号」得用 `\;`（见 2.4.2 的转义表）。
- **unquoted 展开会把列表摊开成多参数，quoted 不会**（2.4.3 已详述，这是列表行为的核心）。

### 2.6.3 `list()` 子命令预览

`list()` 是操作列表的瑞士军刀，常用子命令一览（**完整用法详见第 08 章「列表与字符串操作」**）：

| 子命令 | 作用 | 例 |
|---|---|---|
| `list(APPEND L ...)` | 末尾追加元素 | `list(APPEND L d e)` |
| `list(PREPEND L ...)` | 开头插入（3.15+） | `list(PREPEND L z)` |
| `list(GET L <idx>... out)` | 按下标取元素（支持负数） | `list(GET L 0 first)` |
| `list(LENGTH L out)` | 取长度 | `list(LENGTH L n)` |
| `list(REMOVE_ITEM L <v>...)` | 按值删除 | `list(REMOVE_ITEM L b)` |
| `list(REMOVE_AT L <idx>...)` | 按下标删除 | `list(REMOVE_AT L 0)` |
| `list(INSERT L <idx> ...)` | 指定位置插入 | `list(INSERT L 1 x)` |
| `list(FIND L <v> out)` | 查下标，找不到为 -1 | `list(FIND L c idx)` |
| `list(JOIN L <sep> out)` | 用分隔符连成字符串 | `list(JOIN L ", " s)` |
| `list(SORT L ...)` | 排序（可 CASE/ORDER 选项） | `list(SORT L)` |
| `list(REVERSE L)` | 反转 | `list(REVERSE L)` |
| `list(REMOVE_DUPLICATES L)` | 去重 | |
| `list(FILTER L {INCLUDE\|EXCLUDE} REGEX <re>)` | 正则筛选（3.6+） | `list(FILTER L EXCLUDE REGEX "^_")` |

```cmake
set(L a b c)
list(APPEND L d)        # L = a;b;c;d
list(LENGTH L n)        # n = 4
list(GET L 0 first)     # first = a
list(GET L -1 last)     # last = d（负数从末尾数）
list(JOIN L " / " s)    # s = "a / b / c / d"
message("${s}")
```

> ⚠️ `list()` **就地修改**第一个参数（那个变量），不返回新列表。`list(GET ...)`、`list(LENGTH ...)`、`list(JOIN ...)` 这种「取结果」的子命令则把结果写到最后的输出变量名里。

### 2.6.4 `string()` 预览

`string()` 处理字符串：拼接、子串、查找替换、正则、大小写转换、JSON、哈希等（**完整见第 08 章**）。先认个脸：

```cmake
string(TOUPPER "hello" UP)               # UP = HELLO
string(LENGTH "hello" LEN)               # LEN = 5
string(SUBSTRING "hello" 1 3 SUB)        # SUB = ell（起点1，长度3）
string(REPLACE "l" "L" OUT "hello")      # OUT = heLLo
string(REGEX MATCH "[0-9]+" M "v123abc") # M = 123
string(APPEND BUF "more")                # 就地把 "more" 追加到 BUF
message("${UP} ${LEN} ${SUB} ${OUT} ${M}")
```

同样地，`string()` 的多数子命令把结果写到指定的输出变量；`string(APPEND/PREPEND ...)` 是就地修改。

---

## 2.7 布尔与条件常量

`if()`、`while()` 需要把参数解读为「真 / 假」。CMake 的真假判定有一张**明确的常量表**，背下来能省掉无数调试时间。

### 2.7.1 真假判定全表

把一个**裸常量值**（或变量短形式）当布尔解读时：

| 判为 **真（true）** | 判为 **假（false）** |
|---|---|
| `ON` | `OFF` |
| `YES` | `NO` |
| `TRUE` | `FALSE` |
| `Y` | `N` |
| 任意**非零数字**（`1`、`2`、`-1`、`3.14`…） | `0` |
| | 空字符串 `""` |
| | `IGNORE` |
| | `NOTFOUND` |
| | **任何以 `-NOTFOUND` 结尾的字符串**（如 `Foo-NOTFOUND`） |

**关键补充规则**：

- **真/假关键字大小写不敏感**：`on`、`On`、`ON`、`true`、`True` 都行。
- **以 `-NOTFOUND` 结尾一律为假**：这是 `find_*` 命令找不到东西时返回值（如 `LIB-NOTFOUND`）的统一约定，所以 `if(MYLIB)` 在没找到库时自然为假，无需特判。
- **上述「常量」之外的其它字符串**（比如 `hello`、`/usr/lib`），在 `if(<single>)` 短形式下会**被当作变量名去解引用**，再用解引用后的值套这张表判断（见下一节 CMP0054）。

```cmake
if(ON)        # 真
endif()
if(0)         # 假
endif()
if("")        # 假（空串）
endif()
if(SomeLib-NOTFOUND)   # 假（以 -NOTFOUND 结尾）
endif()
```

---

## 2.8 控制流

CMake 的控制流命令都是**成对的块命令**：`if/endif`、`foreach/endforeach`、`while/endwhile`、`function/endfunction`、`macro/endmacro`、`block/endblock`。结束命令可以带（可选的）和开始一致的标识，纯属可读性，新代码一般留空。

### 2.8.1 `if / elseif / else / endif`

```cmake
if(<condition>)
    # ...
elseif(<condition>)
    # ...
else()
    # ...
endif()
```

#### 条件运算符完整表

`if()` 的条件由「运算符 + 操作数」组成。按**求值优先级从高到低**：括号 `()` → 一元判定 → 二元比较 → `NOT` → `AND`/`OR`（左到右，**不短路**）。

**① 一元判定（unary，作用于其右侧单个操作数）**

| 运算符 | 为真的条件 |
|---|---|
| `DEFINED <var>` | 变量（或 `CACHE{var}` / `ENV{var}`）**已定义**（不管值） |
| `COMMAND <name>` | 存在名为 `<name>` 的命令/函数/宏 |
| `POLICY <CMPxxxx>` | 存在该策略 |
| `TARGET <name>` | 存在该构建目标（由 `add_executable/library/custom_target` 定义） |
| `TEST <name>` | 存在该测试（`add_test` 定义） |
| `EXISTS <path>` | 文件或目录存在 |
| `IS_READABLE <path>` | 路径可读（3.29+） |
| `IS_WRITABLE <path>` | 路径可写（3.29+） |
| `IS_EXECUTABLE <path>` | 路径可执行（3.29+） |
| `IS_DIRECTORY <path>` | 路径是目录 |
| `IS_SYMLINK <path>` | 路径是符号链接 |
| `IS_ABSOLUTE <path>` | 是绝对路径 |

**② 二元比较（binary，左右各一个操作数）**——注意分「数值 / 字符串 / 版本」三套：

| 类别 | 运算符 | 含义 |
|---|---|---|
| **数值** | `EQUAL` `LESS` `GREATER` `LESS_EQUAL` `GREATER_EQUAL` | 当作数字比较（`LESS_EQUAL`/`GREATER_EQUAL` 自 3.7） |
| **字符串** | `STREQUAL` `STRLESS` `STRGREATER` `STRLESS_EQUAL` `STRGREATER_EQUAL` | 按字典序比较字符串 |
| **版本号** | `VERSION_EQUAL` `VERSION_LESS` `VERSION_GREATER` `VERSION_LESS_EQUAL` `VERSION_GREATER_EQUAL` | 按 `a.b.c.d` 语义比较版本 |
| **正则** | `MATCHES <regex>` | 左操作数匹配正则；捕获组存入 `CMAKE_MATCH_<n>` |
| **路径** | `PATH_EQUAL` | 规范化后路径相等（3.24+，受策略 CMP0139 控制） |
| **列表** | `<item> IN_LIST <listvar>` | `<item>` 是列表 `<listvar>` 的元素之一（3.3+） |
| **文件时间** | `<a> IS_NEWER_THAN <b>` | a 不比 b 旧（注意语义：相等或更新都为真；建议只用于文件） |

**③ 逻辑运算（boolean）**

| 运算符 | 含义 |
|---|---|
| `NOT <cond>` | 取反 |
| `<a> AND <b>` | 与 |
| `<a> OR <b>` | 或 |

> ⚠️ **不短路**：CMake 的 `AND`/`OR` 会求值两侧全部子表达式，不像 C 那样左边为假就跳过右边。所以 `if(DEFINED P AND EXISTS ${P})` 这种「先判定义再用」的写法**不能保证安全**，必要时拆成嵌套 `if`。

#### 操作数的自动解引用规则（务必理解）

这是 `if()` 最微妙的地方。对于 `MATCHES/LESS/GREATER/EQUAL/STREQUAL/VERSION_*/IN_LIST` 这类「接受 `<variable|string>`」的比较运算符：**操作数会先被当作变量名查找——若该名字是已定义变量，用其值；否则按字面字符串用。**

```cmake
set(A 10)
set(B 20)
if(A LESS B)            # A、B 都是已定义变量 → 用值 10 < 20 → 真
    message("10 < 20")
endif()

if("A" STREQUAL "B")    # 引号字面量，不当变量 → 比较字符串 "A" 与 "B" → 假
endif()
```

而 `AND`/`OR` 的操作数先按布尔常量判定，不是常量再当变量解引用；`NOT` 的右操作数同理。

#### `if(<single>)` 短形式与 CMP0054（引号语义）

`if(X)` 只给一个操作数时，是「真假判定」的短形式：**把 `X` 当变量名解引用，对其值套 2.7.1 那张真假表**。这正是为什么 `if(BUILD_TESTS)` 能直接判断开关的原因。

**策略 CMP0054（CMake 3.1 引入）**改变了「带引号的操作数还会不会被当变量解引用」：

- **`CMP0054` 为 `NEW`（现代默认）**：**用引号括起来的操作数一律按字面字符串处理，绝不再解引用。** 这是符合直觉的行为。
- **`OLD`（2.x 旧行为）**：即使加了引号，若内容恰好是某变量名，仍会被解引用——容易出诡异 bug。

```cmake
# 在 CMP0054=NEW（即 cmake_minimum_required(VERSION 3.1) 及以上）下：
set(FOO BAR)
set(BAR xyz)

if(FOO STREQUAL "BAR")   # 右侧带引号 → 字面量 "BAR"；左侧 FOO 解引用为 "BAR" → 真
    message("FOO 的值等于字符串 BAR")
endif()

if(FOO STREQUAL BAR)     # 右侧无引号、是变量名 BAR → 解引用为 "xyz"；FOO→"BAR" → 假
endif()
```

> 📌 **实践建议**：在 `STREQUAL`/`MATCHES` 等比较里，**想比字面字符串就给操作数加引号**（`if(X STREQUAL "Debug")`），配合 `CMP0054=NEW` 行为最清晰、最不易踩坑。只要你的 `cmake_minimum_required(VERSION ...)` ≥ 3.1，CMP0054 就是 NEW，无需手动设。

### 2.8.2 `foreach`

`foreach` 有多种遍历形态。

**① 直接列出 / 遍历变量展开**

```cmake
foreach(item a b c)          # 直接给出
    message("- ${item}")
endforeach()

set(L x y z)
foreach(item ${L})           # 经典写法：把列表 unquoted 展开后逐个遍历
    message("- ${item}")
endforeach()
```

**② `RANGE` 数值范围**

```cmake
foreach(i RANGE 5)           # 0 1 2 3 4 5（含两端！共 6 个）
endforeach()

foreach(i RANGE 1 10)        # 1..10
endforeach()

foreach(i RANGE 0 10 2)      # 起 0 止 10 步长 2 → 0 2 4 6 8 10
endforeach()
```

> ⚠️ `RANGE <stop>` 是 **0 到 stop 闭区间**（包含 stop），和很多语言「不含上界」相反，极易记错。

**③ `IN LISTS`（推荐，最安全）**

```cmake
set(L a b c)
foreach(item IN LISTS L)     # 传「变量名」，不展开，能正确处理空元素
    message("- ${item}")
endforeach()

# 可同时遍历多个列表（首尾相接）
set(M 1 2)
foreach(item IN LISTS L M)   # a b c 1 2
endforeach()
```

`IN LISTS` 接的是**变量名**（写 `L` 不写 `${L}`），比 `foreach(x ${L})` 更稳——后者在列表含空元素时会丢项。**新代码优先用 `IN LISTS`。**

**④ `IN ITEMS`（直接给元素，可与变量混用）**

```cmake
foreach(item IN ITEMS apple banana ${L})   # 把后面的当「元素」逐个给
endforeach()
```

**⑤ `IN ZIP_LISTS`（多列表并行遍历，CMake 3.17+）**

```cmake
set(NAMES  alice bob carol)
set(AGES   30 25 40)
foreach(name age IN ZIP_LISTS NAMES AGES)   # 给两个循环变量，并行取
    message("${name} 今年 ${age} 岁")
endforeach()
# 若只给一个循环变量 v，则用 v_0 / v_1 ... 访问各列表当前元素
```

### 2.8.3 `while`、`break()`、`continue()`

```cmake
set(i 0)
while(i LESS 5)              # 条件语法与 if 完全一致
    message("i = ${i}")
    math(EXPR i "${i} + 1")  # 自增（CMake 没有 ++）
endwhile()
```

- **`break()`**：立即跳出最内层 `foreach`/`while`。
- **`continue()`**：跳过本次迭代剩余部分，进入下一次（3.2+）。

```cmake
foreach(i RANGE 10)
    if(i EQUAL 3)
        continue()          # 跳过 3
    endif()
    if(i EQUAL 6)
        break()             # 到 6 整个循环结束
    endif()
    message("i = ${i}")     # 打印 0 1 2 4 5
endforeach()
```

---

## 2.9 函数与宏

CMake 提供两种「自定义命令」机制：**`function()`（函数）** 与 **`macro()`（宏）**。二者长得像，行为差别却很大，选错会导致诡异的作用域 bug。

### 2.9.1 `function()` 函数

```cmake
function(<name> [<arg1> <arg2> ...])
    # 函数体
endfunction()
```

```cmake
function(greet who)
    message("你好，${who}！")
endfunction()

greet("世界")            # 调用：输出 你好，世界！
```

- **拥有独立的变量作用域**：函数体里 `set()` 出的普通变量默认**只在函数内可见**，调用结束即销毁，不污染调用者。要把结果传回去，用 `set(... PARENT_SCOPE)`（见 2.10）。
- 形参在函数内作为普通变量存在。

### 2.9.2 `macro()` 宏

```cmake
macro(<name> [<arg1> ...])
    # 宏体
endmacro()
```

```cmake
macro(set_default var val)
    if(NOT DEFINED ${var})
        set(${var} ${val})   # 直接写到调用者作用域（宏无独立作用域）
    endif()
endmacro()

set_default(MY_OPT 42)       # 调用后 MY_OPT 在外层就有了
message("${MY_OPT}")         # 42
```

- **没有独立作用域**——宏是**文本替换**：调用处仿佛把宏体原地展开，所有 `set()` 直接作用于调用者当前作用域。
- 因为是文本替换，宏的「参数」也不是真正的变量，而是**字符串替换占位符**，这带来若干陷阱（见 2.9.4）。

### 2.9.3 隐式参数变量：`ARGC` / `ARGV` / `ARGN` / `ARGVn`

函数和宏内部都能访问这些自动变量，用于处理可变参数：

| 变量 | 含义 |
|---|---|
| `ARGC` | 实际传入的**参数总个数** |
| `ARGV0` `ARGV1` … | 第 0、1、… 个参数（按位置） |
| `ARGV` | **所有**实参组成的列表 |
| `ARGN` | **命名形参之后**多出来的实参列表（即「额外参数」） |

```cmake
function(demo first)
    message("ARGC=${ARGC}")     # 总数
    message("ARGV0=${ARGV0}")   # = first
    message("ARGN=${ARGN}")     # 超出 first 的那些
endfunction()

demo(a b c)
# ARGC=3  ARGV0=a  ARGN=b;c
```

`ARGN` 是写「接受任意多个尾随参数」的命令的关键。

### 2.9.4 function vs macro 对比

| 维度 | `function()` | `macro()` |
|---|---|---|
| **作用域** | **独立作用域**，内部 `set` 不外泄 | **无独立作用域**，等同把代码贴到调用处 |
| **结果回传** | 需 `set(VAR ... PARENT_SCOPE)` | `set(VAR ...)` 直接改调用者变量 |
| **参数本质** | 真正的局部变量（`who`、`ARGV0`…） | 文本占位替换，**不是变量**（`${who}` 才是替换点，`if(who)` 之类不靠谱） |
| **`ARGN` 等** | 是真正变量，可 `foreach(x IN LISTS ARGN)` | `ARGN`/`ARGV` 是替换串，**不能** `IN LISTS` 遍历（要 `foreach(x ${ARGN})`） |
| **`return()`** | 提前返回函数 | `return()` 会从**调用者**返回（因无自己的帧），危险 |
| **典型用途** | 封装逻辑、需要局部变量、绝大多数场景 | 只在「必须修改调用者多个变量」「需要把代码原样插入」时用 |

> 📌 **选择准则**：**默认一律用 `function()`。** 只有当你确实需要「直接、批量地修改调用者作用域里的变量」或「把一段代码字面插入调用点」时，才用 `macro()`，并且时刻警惕它的文本替换语义（尤其别在宏里用 `if(ARGV0)` 这种把替换串当布尔/变量的写法）。

### 2.9.5 `cmake_parse_arguments()` 解析命名参数

当自定义命令的参数变多，靠 `ARGV0`/`ARGV1` 位置取参既丑又脆。`cmake_parse_arguments()` 让你像内置命令那样支持 **关键字参数**（`PUBLIC`、`SOURCES x y z`、`VERSION 1.0` 这种风格）。

```cmake
cmake_parse_arguments(
    <prefix>
    "<options>"            # 布尔开关关键字（出现即真）
    "<one_value_keywords>" # 后接 1 个值的关键字
    "<multi_value_keywords>"# 后接多个值的关键字
    ${ARGN})
```

```cmake
function(add_module name)
    cmake_parse_arguments(
        ARG                       # 前缀：解析结果都以 ARG_ 开头
        "SHARED;EXCLUDE_FROM_ALL" # options（布尔）
        "VERSION"                 # 单值
        "SOURCES;DEPENDS"         # 多值
        ${ARGN})

    # 解析后可直接用：
    #   ARG_SHARED        → TRUE/FALSE（开关是否出现）
    #   ARG_VERSION       → 单值
    #   ARG_SOURCES       → 列表
    #   ARG_UNPARSED_ARGUMENTS    → 没被识别的多余参数
    #   ARG_KEYWORDS_MISSING_VALUES → 给了关键字却没给值的那些（3.15+）

    message("name=${name} shared=${ARG_SHARED} ver=${ARG_VERSION}")
    message("sources=${ARG_SOURCES} depends=${ARG_DEPENDS}")
endfunction()

add_module(net
    SHARED
    VERSION 1.2
    SOURCES net.cpp socket.cpp
    DEPENDS ssl)
# name=net shared=TRUE ver=1.2
# sources=net.cpp;socket.cpp depends=ssl
```

这是写「像样的、可复用的」CMake 函数的标配，几乎所有成熟项目的辅助函数都用它。

---

## 2.10 作用域

作用域决定「一个变量在哪儿可见、改它影响谁」。CMake 有**三种作用域**外加一个块作用域，理清它们能根治「为什么我设的变量在外面取不到 / 在里面改了外面没变」。

### 2.10.1 三种作用域

1. **目录作用域（Directory scope）**：每个 `add_subdirectory()` 进入的目录是一个新作用域。**子目录会拷贝一份父目录此刻的全部普通变量**（快照式继承），但子目录里对变量的修改**默认不会回传父目录**。
2. **函数作用域（Function scope）**：`function()` 调用时新建，同样能读到外层变量（按调用栈），内部 `set` 默认局部。**`macro()` 不创建此作用域。**
3. **块作用域（Block scope）**：`block()`/`endblock()`（3.25+）显式创建的临时作用域。

缓存变量与环境变量是**全局**的，不受上述作用域限制。

### 2.10.2 `set(... PARENT_SCOPE)`

在函数或子目录里，把变量写回**上一层**作用域：

```cmake
function(compute out)
    set(${out} 42 PARENT_SCOPE)   # 把结果写到调用者作用域里名为 ${out} 的变量
endfunction()

compute(RESULT)
message("${RESULT}")              # 42
```

要点：

- `PARENT_SCOPE` 只影响**父作用域**，**不影响当前作用域**。即在函数内 `set(X 1 PARENT_SCOPE)` 之后，函数内自己的 `${X}` **仍是旧值/未定义**——它写的是外面那层。需要内外都用就写两次。
- 子目录里 `set(X v PARENT_SCOPE)` 把值送回父目录。
- 这是函数「返回结果」的标准手段（把输出变量名作参数传进来，再 `set(${out} ... PARENT_SCOPE)`）。

### 2.10.3 目录作用域继承（`add_subdirectory`）

```cmake
# 顶层 CMakeLists.txt
set(TOP_VAR "from-top")
add_subdirectory(sub)          # 进入子目录：sub 能看到 TOP_VAR 的当前值
message("${SUB_BACK}")         # 取子目录回传的值（见下）

# sub/CMakeLists.txt
message("${TOP_VAR}")          # from-top（继承自父）
set(TOP_VAR "changed")         # 仅改子目录这份拷贝，父目录不受影响
set(SUB_BACK "hello" PARENT_SCOPE)  # 显式回传给父
```

记住继承是**单向快照**：父→子自动可见（值是进入子目录那一刻的快照），子→父必须显式 `PARENT_SCOPE`。

### 2.10.4 `block() / endblock()` 与 `return(PROPAGATE)`（现代写法，3.25+）

`block()` 提供更精细、更直观的作用域控制，是对 `PARENT_SCOPE` 这种「数着层数往上传」笨办法的现代化替代。

```cmake
block([SCOPE_FOR [POLICIES] [VARIABLES]] [PROPAGATE <var>...])
    # 块内代码：新建变量作用域（默认），出块即销毁块内 set 的变量
endblock()
```

```cmake
set(X outer)
block()
    set(X inner)          # 只在块内
    set(Y temp)           # 出块即消失
    message("${X}")       # inner
endblock()
message("${X}")           # outer（块内修改不外泄）
# message("${Y}") 这里 Y 已不存在

# 用 PROPAGATE 显式把指定变量传到外层（替代 PARENT_SCOPE）
block(PROPAGATE RESULT)
    set(RESULT computed)
endblock()
message("${RESULT}")      # computed
```

- **`SCOPE_FOR VARIABLES`**：只为变量新建作用域；**`SCOPE_FOR POLICIES`**：只为策略新建作用域。不写 `SCOPE_FOR` 则两者都新建。
- **`PROPAGATE <var>...`**：块结束时把这些变量的当前值（或「未定义」状态）同步到父作用域，等价于在块里对它们 `set(... PARENT_SCOPE)`，但写法集中、清晰。

**`return(PROPAGATE <var>...)`（3.25+，受 CMP0140 控制）**：让函数在 `return` 的同时把变量传回调用者，且能**穿透函数内部任意层 `block()`**：

```cmake
function(find_value out)
    block()
        set(tmp 99)
        return(PROPAGATE ${out})   # 把 ${out} 传回函数的调用者（穿过这个 block）
    endblock()
endfunction()
```

> `return(PROPAGATE ...)` 的参数仅在策略 **CMP0140** 为 `NEW` 时才生效（`cmake_minimum_required(VERSION 3.25)` 以上自动 NEW）；否则参数被忽略。普通的 `return()`（无参数）任何版本都只是「提前退出函数」。

---

## 2.11 `include()` 与脚本复用、`message()` 日志

### 2.11.1 `include()`：把别的 .cmake 拉进当前作用域执行

```cmake
include(<file|module> [OPTIONAL] [RESULT_VARIABLE <var>] [NO_POLICY_SCOPE])
```

`include()` 读取并**在当前作用域、当前位置「就地执行」**目标文件——**不创建新作用域**（这点和 `add_subdirectory` 截然不同：`include` 进来的 `set()` 直接作用于当前作用域，如同把内容粘贴过来）。

```cmake
include(cmake/Helpers.cmake)        # 引入项目自带的 .cmake 文件（相对当前文件目录）
include(GNUInstallDirs)             # 引入 CMake 自带模块（按模块名查找）

include(maybe.cmake OPTIONAL RESULT_VARIABLE done)
if(NOT done)                        # done 为 NOTFOUND 说明文件不存在（OPTIONAL 不报错）
    message(STATUS "可选脚本未找到，跳过")
endif()
```

- **两种用法**：给**文件路径**则按路径加载；给**模块名**（不带 `.cmake` 后缀）则去 `CMAKE_MODULE_PATH` 和 CMake 内置模块目录里找 `<名字>.cmake`。
- **`OPTIONAL`**：文件不存在也不报错。
- **`RESULT_VARIABLE`**：把实际加载的完整路径写入变量；失败时为 `NOTFOUND`。
- **共享代码的基本手段**：把可复用的函数/宏放进 `*.cmake`，各处 `include()` 进来即可。注意，由于 include 不开新作用域，被包含文件里定义的函数、设置的变量都会落到包含它的作用域。

> `include()` 用于**脚本片段**复用；引入「一个完整的库依赖」用 `find_package()`，引入子项目用 `add_subdirectory()`。三者别混。

### 2.11.2 `message()`：日志与诊断

`message()` 输出信息，并通过**模式关键字**控制级别与行为。级别既影响显示位置（stdout/stderr）、是否带前缀，也受 `--log-level` 过滤、决定是否中断构建。

```cmake
message([<mode>] "文本..." ...)
```

| 模式 | 行为 | 是否中断 |
|---|---|---|
| `FATAL_ERROR` | 报致命错误并**立即停止** cmake 处理 | ✅ 终止 |
| `SEND_ERROR` | 报错、**继续**执行到配置结束，但**不会进入生成阶段** | ⚠️ 配置后停 |
| `WARNING` | 警告（输出到 stderr），继续 | ❌ |
| `AUTHOR_WARNING` | 给项目作者看的警告（可被 `-Wno-dev` 关掉） | ❌ |
| `DEPRECATION` | 弃用提示（受 `CMAKE_WARN_DEPRECATED` 等控制） | ❌ |
| `NOTICE`（缺省无模式时） | 普通消息，输出到 **stderr**，不带 `--` 前缀 | ❌ |
| `STATUS` | 状态信息，带 `-- ` 前缀，输出到 stdout，**最常用** | ❌ |
| `VERBOSE` | 详细信息，默认不显示，`--log-level=VERBOSE` 才出 | ❌ |
| `DEBUG` | 调试信息，`--log-level=DEBUG` 才出 | ❌ |
| `TRACE` | 最细粒度，`--log-level=TRACE` 才出 | ❌ |

```cmake
message(STATUS "正在配置 ${PROJECT_NAME} ${PROJECT_VERSION}")
message(VERBOSE "使用编译器：${CMAKE_CXX_COMPILER}")

if(NOT DEFINED REQUIRED_VAR)
    message(FATAL_ERROR "必须提供 REQUIRED_VAR，例如 -DREQUIRED_VAR=...")
endif()
```

**级别过滤**：命令行 `cmake --log-level=VERBOSE`（或缓存变量 `CMAKE_MESSAGE_LOG_LEVEL`）控制最低显示级别。日志级别从高到低为：`ERROR`(对应 FATAL/SEND) > `WARNING` > `NOTICE` > `STATUS` > `VERBOSE` > `DEBUG` > `TRACE`。

> 还可用 `message(CHECK_START ...)` / `CHECK_PASS` / `CHECK_FAIL` 配合 `CMAKE_MESSAGE_INDENT` 打印带缩进的「检查中…完成」式分组日志，让配置输出更清爽（细节见日志相关章节）。

---

## 本章小结

- **执行模型**：listfile（`CMakeLists.txt` / `*.cmake`）是命令式脚本，**逐条命令、自上而下求值**，靠副作用工作，顺序至关重要；本章所有语法都发生在**配置阶段**。
- **注释**：行注释 `#`、块注释 `#[[ ... ]]`（含内容时加等号 `#[=[ ]=]`）。
- **命令调用**：`命令名(参数...)`，命令名**大小写不敏感（约定小写）**、`(` 紧跟命令名、**参数用空白分隔不是逗号**。
- **三种参数**：**bracket `[[ ]]`** 原样不展开；**quoted `"..."`** 展开变量/转义、永远单参数；**unquoted** 展开变量/转义、**结果含 `;` 会拆成多参数**。「要摊开列表用 unquoted，要保整体用 quoted」。
- **三类变量**：普通 `${X}`（作用域局部）、缓存 `$CACHE{X}`（持久化）、环境 `$ENV{X}`；**未定义变量求值为空串**，`DEFINED` 判存在与否（与值是否为空无关）；`${${x}}` 可嵌套。
- **一切皆字符串**，**列表 = 分号分隔的字符串**；`list()`/`string()` 子命令就地或写出结果（详见第 08 章）。
- **真假表**：`ON/YES/TRUE/Y/非零` 为真；`OFF/NO/FALSE/N/0/空/IGNORE/NOTFOUND/*-NOTFOUND` 为假。
- **控制流**：`if/elseif/else`（完整运算符表 + 自动解引用 + **CMP0054** 引号语义）、`foreach`（`RANGE` 闭区间 / `IN LISTS` 推荐 / `IN ITEMS` / `IN ZIP_LISTS` 3.17+）、`while` + `break`/`continue`。
- **函数 vs 宏**：`function()` **有独立作用域**（默认首选），`macro()` 是**文本替换无作用域**；`ARGC/ARGV/ARGN/ARGVn` 处理可变参数；`cmake_parse_arguments()` 解析关键字参数。
- **作用域**：目录（快照继承、子改不回传）、函数、`block()`（3.25+）；回传用 `set(... PARENT_SCOPE)` 或 `block(PROPAGATE)` / `return(PROPAGATE)`。
- **复用与日志**：`include()` **就地执行不开新作用域**（区别于 `add_subdirectory`）；`message()` 的级别 `FATAL_ERROR`/`SEND_ERROR`/`WARNING`/`STATUS`/`VERBOSE`/`DEBUG`/`TRACE` 决定中断与显示，受 `--log-level` 过滤。

掌握本章后，你已经能读懂任意 CMake 脚本的语法。下一章我们把这些语法用起来，**从零写出第一个真正的 CMake 项目并跑通完整构建流程**。

---

> ⬅️ [[42.Cmake/01 - CMake 概述与安装.md|上一章]] ｜ ➡️ [[42.Cmake/03 - 第一个项目与构建流程.md|第 03 章]]
>
> 🏠 [[00 - CMake 完整技术教程 - 总索引|总索引]]
