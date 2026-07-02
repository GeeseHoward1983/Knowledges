# 46.构建系统横向对比 — 选题广度审查报告

**审查日期**：2026-07-01
**现有篇数**：13 篇（01–13）
**审查方法**：对照构建系统完整知识地图，逐域扫描覆盖空白

---

## 现有覆盖摘要

| 现有篇 | 覆盖域 |
|--------|--------|
| 01 | 概论 / 分类框架 |
| 02 | Make |
| 03 | CMake |
| 04 | Bazel（单仓/沙盒/RBE） |
| 05 | Ninja（后端） |
| 06 | Meson |
| 07 | Gradle / JVM |
| 08 | Cargo / Rust |
| 09 | 依赖管理（vcpkg/Conan/crates.io/Maven） |
| 10 | 增量构建与缓存 |
| 11 | 分布式构建与远程执行 |
| 12 | 供应链安全（SLSA/SBOM） |
| 13 | 性能优化与选型 |

---

## 建议新增篇章

---

### 14. Buck2 与元编程构建语言

Buck2 是 Meta 于 2023 年开源的新一代分布式构建系统，以 Starlark 2 + Prelude 为核心，在执行模型上彻底抛弃 Action Graph 可变状态，改用 Dice（Deterministic Incremental Computation Engine）实现细粒度增量。与 Bazel 同属 Google/Meta 大厂工程文化产物，但设计哲学差异显著（规则即函数 vs 规则即宏）。已在 Meta 内部替换了 Buck1/Bazel，在 C++/Python/Rust 多语言单仓中验证过 10 万+节点构建。单独成篇可与第 04 篇（Bazel）形成对比轴，覆盖 Prelude、transition、BXL 扩展脚本等独有概念。

---

### 15. Nix/Guix 可复现构建与纯函数式包管理

Nix 将构建描述为纯函数（输入哈希 → Nix store 路径），实现跨机器、跨时间的位级可复现性（Reproducible Builds）。NixOS/nix-darwin/nix flakes 已进入主流 DevOps 视野，尤其在嵌入式、HPC、研究可复现性领域成为事实标准。Guix 是 Scheme 实现的类 Nix 变体。现有第 10 篇（增量缓存）和第 12 篇（供应链安全）均未涉及"构建输出内容寻址 + 环境即代码"这一范式，与其他工具的横向对比角度独特。

---

### 16. MSBuild 与 .NET / Visual Studio 构建体系

MSBuild 是 Windows/.NET 生态的事实构建标准，支撑 Visual Studio 全部项目类型（C#/C++/VB/XAML）及 .NET SDK CLI（`dotnet build`）。其 Target/Task 扩展模型、条件属性语法、SDK 样式 csproj、NuGet 集成与现有 JVM 系（Gradle/Maven）形成鲜明对照。Windows C++ 开发者、.NET 平台开发者占全球开发者相当大比例，现有系列完全没有 MSBuild 是明显缺口。

---

### 17. Monorepo 构建工具专章——Nx / Turborepo / Pants / Rush

随着前端/全栈工程向 monorepo 迁移，Nx、Turborepo、Rush 在 JS/TS 生态、Pants 在 Python/Java 生态已成为独立工具品类。其核心价值在于：任务编排（比 Make 更高层）、影响分析（affected graph）、本地 + 远程缓存、项目间发布边界管理。现有第 04 篇（Bazel）虽涉及单仓，但 Bazel 适用范围和这批工具的定位（轻量/语言专用）差别很大，值得单独对比。

---

### 18. 交叉编译专章——工具链配置、Sysroot 与多平台产物

交叉编译（host ≠ target）是嵌入式、移动（Android NDK/iOS）、RISC-V/ARM 服务器迁移的核心痛点。CMake toolchain file、Meson cross file、Bazel platform/transition、Cargo cross 各有不同抽象，sysroot 管理、libc 版本隔离、调试信息剥离等是共性挑战。现有第 03/06 篇虽提到交叉编译支持，但均一笔带过；独立成篇可系统覆盖工具链三元组、multilib、QEMU 仿真测试等完整工作流。

---

### 19. 容器化构建——Docker / BuildKit / OCI 镜像构建系统

Docker BuildKit 的 LLB（Low-Level Build）中间表示、多阶段构建、构建缓存挂载（`--mount=type=cache`）和并行 stage 本质上是一套 DAG 构建系统，与 Ninja/Bazel 在抽象层次上同构。Dockerfile、Earthfile（Earthly）、Dagger（Go/Python SDK）代表容器化构建的不同取径。现有系列完全没有 OCI/容器化构建视角，而这是现代 CI/CD 的基础构建单元。

---

### 20. CI/CD 与构建系统集成——GitHub Actions / GitLab CI / Bazel CI Patterns

构建系统与 CI/CD 平台的集成涉及：缓存分层策略（layer cache vs action cache）、矩阵构建、构建产物上传与版本策略、`--build-event-file` / BuildBuddy 与 CI 的联动。现有第 11 篇（RBE）聚焦分布式执行协议，第 12 篇聚焦安全，但 CI/CD 集成的工程实践（pipeline 设计、构建时间优化、flaky test 隔离）是独立话题，对工程师日常价值最直接。

---

### 21. 构建可观测性——Build Scan / OpenTelemetry / Metrics

Gradle Build Scan、Bazel Build Event Protocol（BEP）、BuildBuddy Invocation Dashboard 代表"构建作为可观测系统"的趋势：构建时间火焰图、动作缓存命中率、临界路径分析、构建日志结构化检索。OpenTelemetry for Builds 是新兴标准。现有第 10 篇（缓存）和第 13 篇（性能）均从机制角度讨论，但构建观测平台的工具链和度量体系是独立工程能力，足以成篇。

---

### 22. 构建产物签名、发布与 Release Engineering

从构建产物到可信发布包，涉及代码签名（Sigstore/cosign）、产物仓库（Artifactory/Nexus/GHCR）、语义化版本 + Changelog 自动化、发布管道 CD 部分。SLSA Level 3/4 的"provenance"需要构建系统直接参与签名流程。现有第 12 篇（供应链安全）侧重攻击面与防护框架，但发布工程（Release Engineering）作为构建系统下游的完整流程，可单独成篇覆盖签名工具链与制品管理。

---

### 23. Bazel 模块化演进——WORKSPACE → Bzlmod 迁移

Bazel 6.0 引入 Bzlmod（MODULE.bazel + Bazel Central Registry），是 Bazel 生态十年来最大依赖管理变革：废弃 `WORKSPACE`、引入模块语义版本、支持多版本选择、消除菱形依赖冲突。现有第 04 篇（Bazel）仍以 WORKSPACE 为主视角描述，而 Bazel 7+ 已默认启用 Bzlmod，大量实际迁移场景需要专篇覆盖 `bazel_dep`、`use_repo`、`override_module`、BCR 发布流程等新范式。

---

### 24. 增量链接器专章——mold / lld / 链接时优化（LTO）

链接是大型 C/C++ 项目构建的最后瓶颈，mold 比 ld.bfd 快 5–12×，lld 是 LLVM 生态标配。链接器选择影响构建系统配置（CMake `CMAKE_LINKER`、Bazel `--linkopt`）、LTO（Thin LTO / Full LTO）与 PGO 的交互、DWARF 调试信息分离（`.dwp`）、动态库延迟绑定。现有系列聚焦构建描述与执行，对编译链路中"链接"这一关键阶段的工具演进和性能优化没有专门覆盖。

---

### 25. SCons / Waf / xmake——Python 嵌入式构建系统比较

SCons（Python Make 替代）、Waf（单文件零依赖 Python 构建）、xmake（Lua DSL，国内 C/C++ 生态活跃）代表"用通用语言写构建逻辑"的一类工具。它们在嵌入式固件、游戏引擎（xmake 在国内游戏行业广泛用）、老旧 C 项目迁移场景占有真实市场。与 Make/CMake/Meson 形成对照，揭示"构建 DSL vs 图灵完备语言"的设计权衡。

---

## 优先级建议

| 优先级 | 篇号 | 标题 | 理由 |
|--------|------|------|------|
| 高 | 16 | MSBuild 与 .NET 构建体系 | 覆盖最大空白：Windows/.NET 生态完全缺席 |
| 高 | 18 | 交叉编译专章 | 嵌入式/移动开发核心痛点，现有各篇均蜻蜓点水 |
| 高 | 14 | Buck2 | Bazel 姊妹系统，2023 年起真正开源成熟，对比价值高 |
| 中 | 19 | 容器化构建 | CI/CD 基础单元，视角完全缺失 |
| 中 | 15 | Nix/Guix | 可复现构建范式独特，与第 12 篇安全主题互补 |
| 中 | 23 | Bzlmod 迁移 | Bazel 用户实际迁移需求紧迫 |
| 中 | 17 | Monorepo 工具 | 前端/全栈工程师高频场景 |
| 低 | 21 | 构建可观测性 | 有价值但可融入现有篇扩展 |
| 低 | 20 | CI/CD 集成 | 部分内容可扩展至第 11/13 篇 |
| 低 | 22 | 产物签名与发布 | 可扩展至第 12 篇 |
| 低 | 24 | 增量链接器 | 偏向编译工具链层，与构建系统有交叠但稍偏 |
| 低 | 25 | SCons/Waf/xmake | 市场份额有限，可作为第 13 篇附录 |

---

*报告生成：Claude Sonnet 4.6 / 2026-07-01*
