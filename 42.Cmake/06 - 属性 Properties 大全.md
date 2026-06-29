---
title: 属性 (Properties) 大全
tags: [cmake, properties, reference]
chapter: "06"
cmake_version: 4.3.4
---

# 第 06 章 · 属性 (Properties) 大全

> 基准版本：CMake 4.3.4

属性（Property）是现代 CMake 的核心数据载体。第 04 章讲的 `target_*` 命令、第 05 章讲的变量与缓存，最终都要落到属性上：变量描述"配置时的临时状态"，而属性描述"附着在某个对象上的、参与最终构建系统生成的持久信息"。可以这样记忆——**你写 `CMakeLists.txt` 的过程，本质上就是在给一堆对象（target、目录、源文件、测试……）设置属性，再让生成器（Generator）把这些属性翻译成 `.vcxproj`、`Makefile`、`build.ninja`。**

本章系统梳理属性的概念、七种作用域、通用读写命令、便捷命令，并给出主流属性的分类速查表。读完本章，你应当能看懂任何一个 `set_property` / `set_target_properties` 调用，并知道去哪里查"这个属性到底叫什么、归哪个作用域管"。

---

## 6.1 属性是什么：与变量的本质区别

属性和变量是 CMake 中最容易混淆的两类"键值对"。它们的差异不在语法，而在**归属（ownership）**与**生命周期**。

### 6.1.1 一句话定义

- **变量（Variable）**：无主，按**作用域（scope）**存在。它属于"当前正在执行的目录/函数",出了作用域就不可见。用 `set()` 写、`${VAR}` 读。详见第 05 章。
- **属性（Property）**：有主，**附着在某个具体对象上**（一个 target、一个目录、一个源文件……）。只要那个对象还在，属性就跟着它走，不受"当前在哪个目录执行"影响。用 `set_property()` / `set_target_properties()` 写、`get_property()` / `get_target_property()` 读。

### 6.1.2 对照表

| 维度 | 变量 (Variable) | 属性 (Property) |
| --- | --- | --- |
| 归属对象 | 无主，挂在作用域上 | 有主，挂在具体对象（target/dir/source/test/cache/install）上 |
| 可见性规则 | 按目录/函数作用域嵌套（子目录继承父目录的副本） | 跟随对象本身；跨目录访问需指明对象（`TARGET tgt`） |
| 读 | `${VAR}` / `$ENV{}` / `$CACHE{}` | `get_property()` / `get_target_property()` / `$<TARGET_PROPERTY:...>` |
| 写 | `set()` / `unset()` | `set_property()` / `set_target_properties()` 等 |
| 追加 | 需手动 `set(X ${X} new)` | 原生 `APPEND` / `APPEND_STRING` |
| 典型用途 | 配置时的临时开关、路径拼接、流程控制 | 描述构建产物的持久特征（输出名、编译选项、链接关系） |
| 生命周期 | 配置阶段，出作用域即失效 | 贯穿配置→生成，写入最终构建系统 |
| 默认值机制 | 无（未定义即空） | 多数属性由对应 `CMAKE_*` 变量在 target 创建时**初始化** |

### 6.1.3 关键认知：`target_*` 命令就是在改 target 属性

这是本章最重要的一条心智模型。下面三组写法**语义几乎等价**：

```cmake
# 写法 A：target_* 命令（推荐）
target_include_directories(mylib PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(mylib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 写法 B：直接操作属性（等价，但啰嗦且易错）
set_property(TARGET mylib APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include)  # 对应 PUBLIC 的"接口"部分
set_property(TARGET mylib APPEND PROPERTY
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include             # 对应 PUBLIC 的"自身"部分
    ${CMAKE_CURRENT_SOURCE_DIR}/src)                                    # 对应 PRIVATE 部分
```

| `target_*` 关键字 | 写入的属性 |
| --- | --- |
| `PRIVATE`（仅自身使用） | 写 `INCLUDE_DIRECTORIES`（非 `INTERFACE_*`） |
| `INTERFACE`（仅传给下游） | 写 `INTERFACE_INCLUDE_DIRECTORIES` |
| `PUBLIC`（自身 + 下游） | **同时**写 `INCLUDE_DIRECTORIES` 和 `INTERFACE_INCLUDE_DIRECTORIES` |

> **由此引出本章末尾的结论**：`target_*` 命令之所以被推荐，正是因为它用 `PUBLIC/PRIVATE/INTERFACE` 帮你把"自身属性"和"接口属性"一次写对，并让依赖关系沿 target 图自动传播；而裸 `set_property` 需要你自己分清写哪个属性、要不要 `APPEND`。详见 6.6 节。

---

## 6.2 属性的七种作用域 (Scope)

每个属性都隶属于一个**作用域**，作用域决定了"这个属性挂在什么对象上"。CMake 把作用域分为以下几类（`set_property` 的第一个参数就是它）：

| 作用域 | 对象 | 名称要求 | 典型属性举例 |
| --- | --- | --- | --- |
| **GLOBAL** | 整个 CMake 运行实例（唯一） | 不接受名称 | `USE_FOLDERS`、`TARGETS`、`ENABLED_LANGUAGES` |
| **DIRECTORY** | 一个目录 | 默认当前目录，可指定 `<dir>` | `INCLUDE_DIRECTORIES`、`ADDITIONAL_CLEAN_FILES`、`SUBDIRECTORIES` |
| **TARGET** | 一个目标（exe/lib） | 必须是已存在的 target | `OUTPUT_NAME`、`CXX_STANDARD`、`LINK_LIBRARIES` |
| **SOURCE** | 一个源文件 | 必须是源文件名 | `LANGUAGE`、`GENERATED`、`COMPILE_OPTIONS` |
| **INSTALL** | 一个已安装文件路径 | 必须是安装文件路径 | `CPACK_*` 相关安装属性 |
| **TEST** | 一个测试（`add_test` 注册的） | 必须是已存在的 test | `WILL_FAIL`、`TIMEOUT`、`LABELS` |
| **CACHE** | 一个缓存条目 | 必须是已存在的 cache entry | `ADVANCED`、`HELPSTRING`、`STRINGS`、`TYPE` |

此外还有两个**只读 / 特殊**的作用域，仅出现在 `get_property` / `define_property` 中，不能用 `set_property` 去"挂"普通属性：

| 特殊作用域 | 说明 |
| --- | --- |
| **VARIABLE** | 唯一作用域，不接受名称。用 `get_property(... VARIABLE PROPERTY <var>)` 查询**变量本身**是否定义（等价于 `if(DEFINED <var>)` 的另一种问法），常配合 `define_property(VARIABLE ...)` 为变量挂文档。 |
| **FILE_SET**（3.23+） | `get_property(... FILE_SET <name> TARGET <tgt> PROPERTY ...)`，查询附着在某 target 上的文件集（File Set，用于 `PUBLIC_HEADER`/`CXX_MODULES` 等现代头文件/模块管理）。 |

### 6.2.1 SOURCE 与 CACHE 作用域的特殊性

- **SOURCE（源文件）作用域**：源文件属性**默认只对"同一目录下创建的 target"可见**。这是 3.18 之前的一个大坑——你在 `a/CMakeLists.txt` 给 `foo.cpp` 设了 `COMPILE_FLAGS`，但 `foo.cpp` 实际被 `b/CMakeLists.txt` 里的 target 编译，属性就不生效。CMake 3.18 起 `set_source_files_properties` 新增 `DIRECTORY` / `TARGET_DIRECTORY` 选项来跨目录设置（见 6.4 节）。
- **CACHE（缓存）作用域**：它操作的不是普通对象，而是缓存条目（第 05 章）的"元数据"——例如 `ADVANCED`（是否在 `ccmake`/GUI 里默认折叠）、`STRINGS`（GUI 下拉框候选值）、`HELPSTRING`（悬停帮助）、`TYPE`（`BOOL`/`PATH`/`FILEPATH`/`STRING`/`INTERNAL`）。改这些属性不改缓存值本身，只改它在 GUI 里的呈现方式。
- **VARIABLE 作用域**：它"属性"的概念是退化的——变量的"值"通过 `${}` 读，而 `get_property(VARIABLE)` 主要用于配合 `define_property` 查询文档或定义状态，实务中很少用。

---

## 6.3 通用读写命令

这一组是底层、全作用域通用的命令。任何属性都能用它们读写，便捷命令（6.4 节）只是它们针对特定作用域的语法糖。

### 6.3.1 `set_property` — 写属性

```cmake
set_property(<SCOPE> [<scope-specifier>...]
             [APPEND] [APPEND_STRING]
             PROPERTY <name> [<value>...])
```

各 `<SCOPE>` 的完整形式：

```cmake
set_property(GLOBAL                          PROPERTY <name> [value...])
set_property(DIRECTORY [<dir>]               PROPERTY <name> [value...])
set_property(TARGET    [<target>...]         PROPERTY <name> [value...])
set_property(SOURCE    [<src>...]
             [DIRECTORY <dirs>...]
             [TARGET_DIRECTORY <targets>...] PROPERTY <name> [value...])  # 后两选项 3.18+
set_property(INSTALL   [<file>...]           PROPERTY <name> [value...])
set_property(TEST      [<test>...] [DIRECTORY <dir>] PROPERTY <name> [value...])  # DIRECTORY 3.28+
set_property(CACHE     [<entry>...]          PROPERTY <name> [value...])
```

参数说明：

| 参数 | 含义 |
| --- | --- |
| `<name>` | 属性名（紧跟在 `PROPERTY` 关键字之后，必填） |
| `<value>...` | 零个或多个值，会被拼成**分号分隔的列表**作为属性值 |
| `APPEND` | 把新值作为**列表**追加到已有值末尾（忽略空值），而非覆盖 |
| `APPEND_STRING` | 把新值作为**字符串**直接拼接到已有值末尾（不加分号），得到更长的字符串 |

> **重要陷阱**：当属性支持 `INHERITED`（继承，见 6.3.3）时，使用 `APPEND` / `APPEND_STRING`，CMake **不会**先去上级作用域取值再追加——它只在当前作用域的已有值上追加。继承只发生在纯 `get_property` 读取时。

示例：

```cmake
# 覆盖式设置：把目标的输出名设为 awesome
set_property(TARGET mylib PROPERTY OUTPUT_NAME "awesome")

# 列表追加：向全局编译特性记录里加目录（INCLUDE_DIRECTORIES 是列表型）
set_property(DIRECTORY APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/extra")

# 字符串追加：往源文件的 COMPILE_FLAGS 后面续一段（注意前导空格）
set_property(SOURCE legacy.c APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-deprecated")

# 一次给多个 target 设同一属性
set_property(TARGET app1 app2 app3 PROPERTY FOLDER "Tools")
```

### 6.3.2 `get_property` — 读属性

```cmake
get_property(<out-var> <SCOPE> [<scope-specifier>...]
             PROPERTY <name>
             [SET | DEFINED | BRIEF_DOCS | FULL_DOCS])
```

各作用域的取值形式与 `set_property` 对称：

```cmake
get_property(<var> GLOBAL                                    PROPERTY <name>)
get_property(<var> DIRECTORY [<dir>]                         PROPERTY <name>)
get_property(<var> TARGET    <target>                        PROPERTY <name>)
get_property(<var> SOURCE    <src> [DIRECTORY <dir> | TARGET_DIRECTORY <tgt>] PROPERTY <name>)
get_property(<var> INSTALL   <file>                          PROPERTY <name>)
get_property(<var> TEST      <test> [DIRECTORY <dir>]        PROPERTY <name>)
get_property(<var> CACHE     <entry>                         PROPERTY <name>)
get_property(<var> VARIABLE                                  PROPERTY <name>)   # 查变量
get_property(<var> FILE_SET  <name> TARGET <tgt>             PROPERTY <name>)   # 查文件集 3.23+
```

末尾的可选关键字决定**返回什么**（默认返回属性值）：

| 关键字 | `<out-var>` 得到的内容 |
| --- | --- |
| 无（默认） | 属性的**值**（未设置则为空） |
| `SET` | 布尔——该属性是否**已被显式设置**（区别于"有默认值但没设过"） |
| `DEFINED` | 布尔——该属性是否经 `define_property` **定义过** |
| `BRIEF_DOCS` | 该属性的**简短文档**字符串 |
| `FULL_DOCS` | 该属性的**完整文档**字符串 |

示例：

```cmake
# 读 target 类型
get_property(t TARGET mylib PROPERTY TYPE)
message(STATUS "mylib 类型是 ${t}")     # 例如 STATIC_LIBRARY

# 判断某 target 是否设过 FOLDER
get_property(has_folder TARGET app SET PROPERTY FOLDER)
if(NOT has_folder)
  set_target_properties(app PROPERTIES FOLDER "Misc")
endif()

# 读全局所有 target 列表
get_property(all_tgts GLOBAL PROPERTY TARGETS)
```

### 6.3.3 `define_property` — 定义自定义属性（含 INHERITED）

当你想发明一个属于自己的属性（比如给 target 打一个项目自定义标签），用 `define_property` 注册它，可附带文档与继承行为。

```cmake
define_property(<SCOPE> PROPERTY <name>
                [INHERITED]
                [BRIEF_DOCS <brief-doc> [<doc>...]]
                [FULL_DOCS  <full-doc>  [<doc>...]]
                [INITIALIZE_FROM_VARIABLE <variable>])
```

`<SCOPE>` 取值：`GLOBAL`、`DIRECTORY`、`TARGET`、`SOURCE`、`TEST`、`VARIABLE`、`CACHED_VARIABLE`。

| 选项 | 含义 |
| --- | --- |
| `INHERITED` | **继承**：当 `get_property` 在当前作用域找不到该属性时，自动向上级作用域链查找（DIRECTORY 找不到→GLOBAL；SOURCE/TARGET/TEST 找不到→其所在 DIRECTORY→GLOBAL）。注意只影响 `get_property` 读；`if(DEFINED)`、`APPEND` 不触发继承。 |
| `BRIEF_DOCS` / `FULL_DOCS` | 简短 / 完整文档，可被 `get_property(... BRIEF_DOCS)` 取回。 |
| `INITIALIZE_FROM_VARIABLE`（3.23+） | 仅 `TARGET` 作用域：新建 target 时用指定变量的当前值初始化该属性。变量名必须含小写字母且不能以 `CMAKE_`/`_CMAKE_` 开头。这是"让自定义 target 属性也拥有 `CMAKE_*` 式默认值"的官方机制。 |

示例：

```cmake
# 定义一个可继承的目录级属性，用来标记"该子树是否启用严格告警"
define_property(DIRECTORY PROPERTY MYPROJ_STRICT_WARNINGS
  INHERITED
  BRIEF_DOCS "是否对该目录及子目录启用 -Werror"
  FULL_DOCS  "在顶层目录设置一次，子目录通过 INHERITED 自动继承，可在子目录覆盖。")

set_property(DIRECTORY PROPERTY MYPROJ_STRICT_WARNINGS ON)   # 顶层开
# 子目录里即使没设，get_property 也会因 INHERITED 读到 ON

# 定义一个带默认值来源的 target 属性
define_property(TARGET PROPERTY MYPROJ_MODULE_TAG
  INITIALIZE_FROM_VARIABLE MYPROJ_module_tag)
set(MYPROJ_module_tag "core")        # 之后 add_library/add_executable 创建的目标都会带上 core
```

---

## 6.4 便捷命令 (Convenience Commands)

下面这些命令是 `set_property` / `get_property` 针对**特定作用域**的语法糖，更短、更常用。要点：**`set_*_properties` 一次能设多个属性（`PROPERTIES <p1> <v1> <p2> <v2> ...`），但都是覆盖式，没有 `APPEND`**；要追加仍需用 6.3 的 `set_property ... APPEND`。

### 6.4.1 Target：`set_target_properties` / `get_target_property`

```cmake
set_target_properties(<target1> [<target2>...]
                      PROPERTIES <prop1> <value1> [<prop2> <value2>]...)

get_target_property(<out-var> <target> <property>)
```

```cmake
set_target_properties(mylib PROPERTIES
    OUTPUT_NAME       awesome
    VERSION           1.2.3
    SOVERSION         1
    CXX_STANDARD      20
    POSITION_INDEPENDENT_CODE ON)

get_target_property(ver mylib VERSION)   # ver == 1.2.3；未设时 ver == ver-NOTFOUND
```

> `get_target_property` 在属性未设置时返回 `<property>-NOTFOUND`（如 `VERSION-NOTFOUND`），可用 `if(ver)` 判定。

### 6.4.2 Directory：`set_directory_properties` / `get_directory_property`

```cmake
set_directory_properties(PROPERTIES <prop1> <value1> [<prop2> <value2>]...)   # 仅作用于当前目录

get_directory_property(<out-var> [DIRECTORY <dir>] <property>)
get_directory_property(<out-var> [DIRECTORY <dir>] DEFINITION <var-name>)     # 读另一目录里某变量的值
```

```cmake
set_directory_properties(PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/gen")

get_directory_property(subs DIRECTORY "${CMAKE_SOURCE_DIR}" SUBDIRECTORIES)   # 子目录列表
```

`DEFINITION <var-name>` 是一个特殊用法：读取**指定目录作用域内某个变量**的值——这是少数能跨目录"窥视变量"的口子。

### 6.4.3 Source：`set_source_files_properties` / `get_source_file_property`

```cmake
set_source_files_properties(<files>...
                            [DIRECTORY <dirs>...]
                            [TARGET_DIRECTORY <targets>...]
                            PROPERTIES <prop1> <value1> [<prop2> <value2>]...)

get_source_file_property(<out-var> <file>
                         [DIRECTORY <dir> | TARGET_DIRECTORY <target>]
                         <property>)
```

| 选项 | 含义（**3.18 新增**） |
| --- | --- |
| `DIRECTORY <dirs>...` | 让属性在**指定目录**的作用域内生效（这些目录必须已被 CMake 处理过）。用于解决"源文件被别的目录的 target 使用"时属性不可见的问题。 |
| `TARGET_DIRECTORY <targets>...` | 让属性在**指定 target 所在目录**的作用域内生效（这些 target 必须已存在）。比手写目录路径更稳。 |

```cmake
# 强制把 .c 当 C++ 编译
set_source_files_properties(legacy.c PROPERTIES LANGUAGE CXX)

# 跨目录：generated.cpp 由 app（定义在别处）编译，需把属性设到 app 所在目录
set_source_files_properties(generated.cpp
    TARGET_DIRECTORY app
    PROPERTIES GENERATED TRUE)

# 给生成的头文件标记，避免 CMake 在配置期因找不到它而报错
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/version.h
    PROPERTIES GENERATED TRUE)
```

### 6.4.4 Test：`set_tests_properties` / `get_test_property`

```cmake
set_tests_properties(<tests>... [DIRECTORY <dir>]
                     PROPERTIES <prop1> <value1> [<prop2> <value2>]...)   # DIRECTORY 3.28+

get_test_property(<test> <property> [DIRECTORY <dir>] <out-var>)
```

```cmake
add_test(NAME parse_test COMMAND parser --self-check)
set_tests_properties(parse_test PROPERTIES
    TIMEOUT  30
    LABELS   "unit;fast"
    WILL_FAIL FALSE)
```

> 测试属性的值**支持生成器表达式**（第 07 章）。测试属性的系统讲解见**第 12 章（测试与 CTest）**，本章 6.5.5 仅做速查。

### 6.4.5 全局便捷读取：`get_cmake_property`

```cmake
get_cmake_property(<out-var> <property>)
```

它是 `get_property(<var> GLOBAL PROPERTY <property>)` 的简写，专读**全局作用域**属性，外加几个伪属性：

| 伪属性 | 返回 |
| --- | --- |
| `VARIABLES` | 当前作用域所有普通变量名列表 |
| `CACHE_VARIABLES` | 所有缓存变量名列表 |
| `COMMANDS` | 所有已定义命令/宏/函数名 |
| `MACROS` | 所有已定义宏名 |
| `COMPONENTS` | 所有 `install(... COMPONENT)` 声明过的组件 |

```cmake
get_cmake_property(all_vars VARIABLES)
foreach(v IN LISTS all_vars)
  message(STATUS "${v} = ${${v}}")     # 调试：打印所有变量
endforeach()
```

### 6.4.6 便捷命令一览

| 作用域 | 设置（多属性，覆盖式） | 读取（单属性） |
| --- | --- | --- |
| GLOBAL | `set_property(GLOBAL ...)` | `get_cmake_property` / `get_property(GLOBAL ...)` |
| DIRECTORY | `set_directory_properties` | `get_directory_property` |
| TARGET | `set_target_properties` | `get_target_property` |
| SOURCE | `set_source_files_properties` | `get_source_file_property` |
| TEST | `set_tests_properties` | `get_test_property` |
| CACHE / INSTALL | （无专用语法糖）`set_property(CACHE/INSTALL ...)` | `get_property(CACHE/INSTALL ...)` |

---

## 6.5 常用属性分类速查

下面按作用域分类列出主流属性。**不求穷尽，但覆盖日常 90% 场景**；完整清单见官方 `cmake-properties(7)`。多数 target 属性都由同名 `CMAKE_<属性>` 变量在 target 创建时初始化（即"全局默认 + 单 target 覆盖"模式）。

### 6.5.1 全局属性 (GLOBAL)

| 属性 | 含义 |
| --- | --- |
| **CMAKE_CXX_KNOWN_FEATURES** | 只读。当前 CMake 已知的全部 C++ 编译特性名列表（如 `cxx_std_20`、`cxx_constexpr`），供 `target_compile_features` 校验。各语言有对应 `CMAKE_<LANG>_KNOWN_FEATURES`。 |
| **ENABLED_LANGUAGES** | 只读。当前已 `enable_language`/`project(... LANGUAGES)` 启用的语言列表（如 `C;CXX`）。 |
| **TARGETS** | 只读。当前已定义的所有逻辑 target 名列表（含 IMPORTED 之外的构建目标）。 |
| **USE_FOLDERS** | 布尔。是否启用"目标分组到文件夹"功能（配合 target 的 `FOLDER` 属性，在 VS / Xcode 里生成树状分组）。CMake 3.26 起默认 `ON`。 |
| **JOB_POOLS** | 定义命名的并发"作业池"（如 `compile=8;link=2`），配合 Ninja 生成器限制特定步骤的并发度，避免链接阶段吃爆内存。 |
| **AUTOGEN_SOURCE_GROUP** | 设置 Qt AUTOMOC/AUTOUIC/AUTORCC 生成文件在 IDE 里归入的 source group 名（另有 `AUTOMOC_SOURCE_GROUP` 等细分）。 |
| **PREDEFINED_TARGETS_FOLDER** | `ALL_BUILD`/`ZERO_CHECK` 等 CMake 自带辅助 target 在 IDE 里归入的文件夹名（默认 `CMakePredefinedTargets`）。 |
| **DEBUG_CONFIGURATIONS** | 声明哪些配置算"调试型"（默认仅 `Debug`），影响 `target_link_libraries(... debug ...)` 等关键字的匹配。 |

### 6.5.2 目标属性 (TARGET) — 重点

target 属性数量最多、最常改。下面分小类列出主流者。

**命名与版本**

| 属性 | 含义 |
| --- | --- |
| **OUTPUT_NAME** | 输出文件的基名（不含前后缀）。不设则用逻辑 target 名。另有按类型/配置细分的 `RUNTIME_OUTPUT_NAME`、`ARCHIVE_OUTPUT_NAME`、`LIBRARY_OUTPUT_NAME`、`<CONFIG>_OUTPUT_NAME`、`OUTPUT_NAME_<CONFIG>`。 |
| **VERSION** | 构建版本号（如 `1.2.3`）。共享库据此生成 `libfoo.so.1.2.3`；Windows/Mach-O 另有解析规则。 |
| **SOVERSION** | 共享库的 ABI 版本（so-name），如 `1` → `libfoo.so.1`。只设其一时另一个默认相同。`NO_SONAME` 置位时被忽略。 |
| **TYPE** | 只读。target 类型：`EXECUTABLE`、`STATIC_LIBRARY`、`SHARED_LIBRARY`、`MODULE_LIBRARY`、`OBJECT_LIBRARY`、`INTERFACE_LIBRARY`、`UTILITY`。 |
| **\<CONFIG\>_POSTFIX** | 某配置下追加到输出名后的后缀，最常见 `set_target_properties(foo PROPERTIES DEBUG_POSTFIX "d")` → 调试版生成 `food.lib`。由变量 `CMAKE_<CONFIG>_POSTFIX` 初始化。 |
| **EXPORT_NAME** | `install(EXPORT)` / `export()` 导出该 target 时使用的名字（替代逻辑名），用于打包对外暴露稳定的别名。 |

**语言标准与编译**

| 属性 | 含义 |
| --- | --- |
| **\<LANG\>_STANDARD** | 语言标准号，如 `CXX_STANDARD 20`、`C_STANDARD 17`。由 `CMAKE_<LANG>_STANDARD` 初始化。 |
| **\<LANG\>_STANDARD_REQUIRED** | 布尔。`ON` 时该标准为**硬要求**，编译器不支持就报错；`OFF`（默认）时会"尽力降级"。建议显式置 `ON`。 |
| **\<LANG\>_EXTENSIONS** | 布尔。是否允许编译器扩展（如 GNU 扩展 `-std=gnu++20` vs 纯净 `-std=c++20`）。默认 `ON`，追求可移植性时置 `OFF`。 |
| **POSITION_INDEPENDENT_CODE** | 布尔。是否生成位置无关代码（PIC，`-fPIC`）。共享库默认 `ON`；要把静态库链进共享库时，静态库也需置 `ON`。由 `CMAKE_POSITION_INDEPENDENT_CODE` 初始化。 |

**包含目录 / 宏定义 / 链接（自身 vs 接口）**

| 属性 | 含义 |
| --- | --- |
| **INCLUDE_DIRECTORIES** | 编译该 target **自身**时的头文件搜索路径（对应 `target_include_directories` 的 `PRIVATE`/`PUBLIC`）。 |
| **INTERFACE_INCLUDE_DIRECTORIES** | 传播给**下游**（依赖本 target 者）的头文件路径（对应 `INTERFACE`/`PUBLIC`）。 |
| **COMPILE_DEFINITIONS** | 编译自身时的预处理宏（如 `FOO=1`），对应 `target_compile_definitions` 的 `PRIVATE`/`PUBLIC`。 |
| **INTERFACE_COMPILE_DEFINITIONS** | 传给下游的宏定义。`COMPILE_OPTIONS` / `INTERFACE_COMPILE_OPTIONS`、`COMPILE_FEATURES` 同理成对。 |
| **LINK_LIBRARIES** | 本 target 链接的库（自身链接需求），对应 `target_link_libraries` 的 `PRIVATE`/`PUBLIC`。 |
| **INTERFACE_LINK_LIBRARIES** | 传给下游的链接需求（`INTERFACE`/`PUBLIC`）。这是"用了我，就也得链上这些库"的核心机制。 |

> 这一组成对属性（`X` 与 `INTERFACE_X`）正是 `PUBLIC/PRIVATE/INTERFACE` 三关键字的底层实现，详见 6.1.3 与 6.6。

**输出目录**

| 属性 | 含义 |
| --- | --- |
| **RUNTIME_OUTPUT_DIRECTORY** | 可执行文件、以及 Windows 下 `.dll` 的输出目录。由 `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 初始化。 |
| **LIBRARY_OUTPUT_DIRECTORY** | 共享库（非 Windows `.so`/`.dylib`）与 MODULE 库的输出目录。 |
| **ARCHIVE_OUTPUT_DIRECTORY** | 静态库 `.a`/`.lib`，以及 Windows 下 DLL 的导入库 `.lib` 的输出目录。 |
| **\<TYPE\>_OUTPUT_DIRECTORY_\<CONFIG\>** | 上述三者的按配置细分版（如 `RUNTIME_OUTPUT_DIRECTORY_RELEASE`）。 |

**IDE 组织 / 安装 / 平台特性**

| 属性 | 含义 |
| --- | --- |
| **FOLDER** | 该 target 在 IDE（VS/Xcode）中所属的虚拟文件夹路径（如 `"Libs/Network"`），需全局 `USE_FOLDERS` 为 `ON`。支持 `/` 分层。 |
| **IMPORTED_LOCATION** | IMPORTED target 在磁盘上主文件的完整路径（`.so`/`.lib`/可执行）。常配 `IMPORTED_LOCATION_<CONFIG>` 按配置区分。`IMPORTED_IMPLIB` 则指 Windows 导入库。 |
| **WIN32_EXECUTABLE** | 布尔。`ON` 时在 Windows 上链接为 GUI 子系统程序（入口 `WinMain`、无控制台窗口）。由 `CMAKE_WIN32_EXECUTABLE` 初始化。 |
| **MACOSX_BUNDLE** | 布尔。`ON` 时在 macOS 上把可执行打成 `.app` 应用程序包（Application Bundle）。 |
| **INSTALL_RPATH** | 安装后写入二进制的 RPATH 列表（运行时动态库搜索路径），如 `$ORIGIN/../lib`。由 `CMAKE_INSTALL_RPATH` 初始化。 |
| **BUILD_RPATH** | 构建树中（未安装时）运行所用的额外 RPATH。配套 `SKIP_BUILD_RPATH`、`BUILD_WITH_INSTALL_RPATH`、`INSTALL_RPATH_USE_LINK_PATH` 等控制 RPATH 策略。 |
| **PUBLIC_HEADER** | 标记为"公开头文件"的列表，`install(TARGETS ... PUBLIC_HEADER DESTINATION)` 会一并安装。 |
| **PRIVATE_HEADER** | 同上，但归为"私有头文件"，安装到不同位置（框架内部）。另有 `RESOURCE`。 |

> 现代项目（3.23+）更推荐用**文件集（File Set）**的 `FILE_SET HEADERS` 替代 `PUBLIC_HEADER` 来管理可安装头文件，能保留目录结构、自动加入 include 路径，详见第 09/11 章。

### 6.5.3 目录属性 (DIRECTORY)

| 属性 | 含义 |
| --- | --- |
| **ADDITIONAL_CLEAN_FILES** | `make clean` / 清理时额外删除的文件/目录列表（如自定义命令产物、生成目录）。 |
| **COMPILE_DEFINITIONS** | 应用于该目录下**所有 target** 的预处理宏（旧式全局加宏的方式，现代更推荐 `target_compile_definitions`）。 |
| **INCLUDE_DIRECTORIES** | 应用于该目录下所有 target 的头文件搜索路径（对应已不推荐的 `include_directories()` 命令）。 |
| **SUBDIRECTORIES** | 只读。经 `add_subdirectory` 加入的直接子目录列表。 |
| **BUILDSYSTEM_TARGETS** | 只读。**在该目录中定义**的非 IMPORTED target 列表（含库、可执行、自定义 target）。常用于"遍历某目录下所有 target 批量设属性"。 |
| **LINK_DIRECTORIES** | 该目录下 target 的库搜索路径（对应 `link_directories()`，同样不推荐，优先用 import target）。 |
| **CMAKE_CONFIGURE_DEPENDS** | 声明额外的文件，其变化将触发 CMake **重新配置**（如读取的外部数据文件）。 |

> 提示：`SUBDIRECTORIES` 给的是"我的直接子目录"，`BUILDSYSTEM_TARGETS` 给的是"在我这个目录里建的 target"，两者配合可递归枚举整棵构建树的全部 target。

### 6.5.4 源文件属性 (SOURCE)

| 属性 | 含义 |
| --- | --- |
| **LANGUAGE** | 强制该源文件按指定语言编译（如把 `.c` 当 `CXX`，或给 `.cu` 指定 `CUDA`）。 |
| **COMPILE_OPTIONS** | 仅编译该文件时附加的选项列表（支持生成器表达式），如 `-O0`。3.11+。 |
| **COMPILE_DEFINITIONS** | 仅该文件可见的预处理宏。 |
| **COMPILE_FLAGS** | 旧式的"编译标志字符串"（空格分隔、不支持生成器表达式），新代码优先用 `COMPILE_OPTIONS`。 |
| **GENERATED** | 布尔。标记该文件由构建过程生成（配置时可能尚不存在），让 CMake 跳过"文件不存在"检查并正确排依赖顺序。 |
| **HEADER_FILE_ONLY** | 布尔。`ON` 时该文件加入工程供 IDE 显示，但**不参与编译**（常用于把头文件挂进 VS 工程树）。 |
| **SKIP_AUTOGEN** | 布尔。让该文件跳过 Qt 的 AUTOMOC/AUTOUIC/AUTORCC 处理（另有 `SKIP_AUTOMOC` 等细分）。 |
| **OBJECT_DEPENDS** | 该文件编译产物所额外依赖的文件列表（如它 `#include` 的生成头文件），变化时触发重编。 |
| **OBJECT_OUTPUTS** | 编译该文件除目标文件外还产生的输出（用于自定义编译规则的依赖跟踪）。 |
| **Fortran_FORMAT** | Fortran 专用，指定 `FIXED`/`FREE` 源码格式。 |

### 6.5.5 测试属性 (TEST) — 简列，详见第 12 章

| 属性 | 含义 |
| --- | --- |
| **WILL_FAIL** | 布尔。`TRUE` 表示"预期失败"——测试返回非零才算通过（反转判定）。 |
| **TIMEOUT** | 该测试的超时秒数，超时即判失败。覆盖全局 `CTEST_TEST_TIMEOUT`。 |
| **LABELS** | 给测试打标签列表，可用 `ctest -L <regex>` 按标签筛选运行。 |
| **DEPENDS** | 声明该测试**运行顺序**依赖的其他测试（仅排序，非"失败即跳过"）。 |
| **FIXTURES_REQUIRED / FIXTURES_SETUP / FIXTURES_CLEANUP** | 测试夹具机制：声明本测试需要某夹具、本测试是某夹具的建立步骤、是清理步骤——CTest 据此自动编排 setup→tests→cleanup。 |
| **ENVIRONMENT** | 运行该测试时设置的环境变量列表（`VAR=value`）。另有 `ENVIRONMENT_MODIFICATION` 做追加/前置等精细操作。 |
| **RESOURCE_LOCK** | 声明该测试独占某命名资源，CTest 并行时确保持锁测试不同时跑。 |
| **RUN_SERIAL** | 布尔。该测试必须单独串行运行，不与任何其他测试并行。 |

> 测试属性的完整用法、CTest 调度模型、夹具与资源分配，集中在**第 12 章（测试、CTest 与 CDash）**。

### 6.5.6 缓存属性 (CACHE) 小结

| 属性 | 含义 |
| --- | --- |
| **TYPE** | 缓存项类型：`BOOL` / `PATH` / `FILEPATH` / `STRING` / `INTERNAL`，决定 GUI 中的编辑控件。 |
| **ADVANCED** | 布尔。`ON` 时该项在 `ccmake`/cmake-gui 默认折叠到"高级"区。对应 `mark_as_advanced()`。 |
| **HELPSTRING** | GUI 中鼠标悬停显示的帮助说明（即 `set(... CACHE ... "这段就是 HELPSTRING")`）。 |
| **STRINGS** | 为 `STRING` 型缓存项提供候选值列表，GUI 渲染成下拉框（不强制校验，仅辅助）。 |

---

## 6.6 属性 vs `target_*` 命令：何时用哪个

这是日常最高频的抉择。结论先行：**优先 `target_*` 命令，只有在 `target_*` 覆盖不到的属性上才回退到 `set_target_properties` / `set_property`。**

### 6.6.1 为什么优先 `target_*`

`target_*` 命令（`target_include_directories`、`target_compile_definitions`、`target_compile_options`、`target_compile_features`、`target_link_libraries`、`target_link_options`、`target_sources` 等）的不可替代价值在于 **`PUBLIC` / `PRIVATE` / `INTERFACE` 三档可见性**：

| 关键字 | 写入属性 | 语义 |
| --- | --- | --- |
| `PRIVATE` | `X` | 仅本 target 编译/链接时用，不传给下游 |
| `INTERFACE` | `INTERFACE_X` | 本 target 自己不用，只传给下游 |
| `PUBLIC` | `X` + `INTERFACE_X` | 本 target 用，且传给下游 |

正是这三档让"**使用需求（usage requirements）沿 target 依赖图自动传播**"成为可能——下游 `target_link_libraries(app PRIVATE mylib)` 后，`mylib` 的 `INTERFACE_INCLUDE_DIRECTORIES`、`INTERFACE_COMPILE_DEFINITIONS`、`INTERFACE_LINK_LIBRARIES` 会自动加到 `app` 上，无需手工同步。这是现代 CMake 的基石。

而 `set_target_properties(mylib PROPERTIES INCLUDE_DIRECTORIES ...)`：

- 是**覆盖式**（容易冲掉之前累积的值），不像 `target_*` 总是 `APPEND`；
- 只写一个属性，你得自己判断该写 `INCLUDE_DIRECTORIES` 还是 `INTERFACE_INCLUDE_DIRECTORIES`，写错就没有传播；
- 完全不参与"使用需求传播"语义。

### 6.6.2 什么时候直接用属性命令

当目标属性**根本没有对应的 `target_*` 命令**、或它本就是"单 target 的固有特征、无传播概念"时，直接 `set_target_properties` 才是正解：

- 输出与命名：`OUTPUT_NAME`、`VERSION`、`SOVERSION`、`DEBUG_POSTFIX`、各 `*_OUTPUT_DIRECTORY`。
- 语言标准开关：`CXX_STANDARD`、`CXX_STANDARD_REQUIRED`、`CXX_EXTENSIONS`（也可用 `target_compile_features` 间接表达需求，但直接设标准更直观）。
- 平台/打包特性：`WIN32_EXECUTABLE`、`MACOSX_BUNDLE`、`FOLDER`、`POSITION_INDEPENDENT_CODE`、`INSTALL_RPATH`、`EXPORT_NAME`、IMPORTED 系列。

### 6.6.3 对照速记

| 场景 | 推荐写法 | 理由 |
| --- | --- | --- |
| 加 include 路径、宏、链接库、编译选项 | `target_*(... PUBLIC/PRIVATE/INTERFACE ...)` | 需要可见性与传播 |
| 设输出名 / 版本 / 后缀 / 输出目录 | `set_target_properties(... PROPERTIES ...)` | 无 `target_*`，且无传播概念 |
| 设语言标准 | `set_target_properties(CXX_STANDARD ...)` 或 `target_compile_features` | 二者皆可，前者直观 |
| 给某属性"追加"而非覆盖 | `set_property(TARGET ... APPEND PROPERTY ...)` | 便捷命令无 `APPEND` |
| 操作 GLOBAL/CACHE/INSTALL 作用域 | `set_property(<SCOPE> ...)` | 无专用便捷命令 |

```cmake
# 典型组合：两类命令各司其职
add_library(net STATIC net.cpp)

# usage requirements → 用 target_*
target_include_directories(net PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(net      PUBLIC  Threads::Threads)
target_compile_features(net    PUBLIC  cxx_std_20)

# 固有特征 → 用 set_target_properties
set_target_properties(net PROPERTIES
    OUTPUT_NAME  mynet
    DEBUG_POSTFIX d
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Libs")
```

---

## 本章小结

- **属性 vs 变量**：变量无主、按作用域；属性有主、附着在具体对象上并贯穿到生成阶段。`target_*` 命令的本质就是改 target 属性。
- **七种作用域**：`GLOBAL` / `DIRECTORY` / `TARGET` / `SOURCE` / `INSTALL` / `TEST` / `CACHE`，外加只读的 `VARIABLE` 与 `FILE_SET`。源文件属性默认只对同目录 target 可见（3.18 起可用 `DIRECTORY`/`TARGET_DIRECTORY` 跨目录）。
- **通用命令**：`set_property`（含 `APPEND`/`APPEND_STRING`）、`get_property`（含 `SET`/`DEFINED`/`*_DOCS`）、`define_property`（含 `INHERITED`、`INITIALIZE_FROM_VARIABLE`）。注意 `APPEND` 不触发继承。
- **便捷命令**：`set_target_properties`/`set_directory_properties`/`set_source_files_properties`/`set_tests_properties` 及对应 `get_*`，均为覆盖式、可一次设多属性，但**没有 `APPEND`**。
- **分类速查**：记住 target 属性里成对的 `X` 与 `INTERFACE_X` 即理解了使用需求传播；其余按"命名/标准/包含链接/输出目录/平台特性"归类查表即可。
- **抉择原则**：需要传播（include/宏/链接/选项）→ `target_*` 命令的 `PUBLIC/PRIVATE/INTERFACE`；单 target 固有特征（输出名/版本/平台/目录）→ `set_target_properties`；全局/缓存/追加 → `set_property`。

> ⬅️ [[42.Cmake/05 - 变量、缓存与作用域.md|上一章]] ｜ ➡️ [[42.Cmake/07 - 生成器表达式.md|第 07 章]]
>
> 🏠 [[00 - CMake 完整技术教程 - 总索引|总索引]]
