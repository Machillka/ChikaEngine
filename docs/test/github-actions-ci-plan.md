# ChikaEngine GitHub Actions CI 计划与实施指南

## 文档状态

- 日期：2026-08-04
- 状态：Core CI Implemented
- 目标分支：`main`
- 适用范围：C++20 引擎、Editor、Game、Benchmark、代码生成工具与 CTest
- 本阶段产物：单一 `.github/workflows/ci.yml`，验证进入 `main` 前后的提交；不配置定时或 nightly 任务

## 当前精简实现

- 触发：向 `main` push、以 `main` 为目标的 Pull Request、手动触发。
- 平台：Ubuntu 24.04/GCC、Windows Server 2022/MSVC、macOS 15/AppleClang。
- 范围：递归 submodule、锁定 Python 环境、全部默认模块及 Game/Editor/Benchmark/tests、全部现有 CTest。
- 门禁：三平台 matrix 汇总为固定检查名 `CI Required`，供 `main` Ruleset 强制要求。
- 约束：无 `schedule`，无 nightly，无发布权限，无图形 smoke；图形 smoke 和新增模块测试仍是后续可选增强。
- 结论：本地 macOS 构建测试通过只能验证 workflow 的本地等价路径，跨平台完成状态以 GitHub Actions 三个 matrix job 的实际结果为准。

## 1. 目标与完成定义

CI 的最终目标不是只验证某个可执行文件能够编译，而是同时证明：

1. 每次 Pull Request 都能在 Linux、Windows、macOS 上完成干净配置。
2. 每个平台都构建所有一方模块以及 `ChikaEngine`、`ChikaGame`、`ChikaEditor`、`ChikaBenchmark` 和全部测试目标。
3. 所有不依赖真实显示器或 GPU 的单元测试、集成测试和架构边界测试都在三个平台运行。
4. Game/Editor/RHI 的图形运行链由独立 smoke 层验证，失败时不会被普通无头测试掩盖。
5. 压力测试、Sanitizer 和 Release 验证有明确执行周期，不拖慢每次小改动的反馈。
6. 合并门禁只在所有必需平台成功后通过；不允许以 `continue-on-error`、隐藏警告或无条件重试把失败变绿。

“所有模块通过”采用两层定义：

- 构建覆盖：每个平台必须编译所有模块和顶层产品目标。
- 行为覆盖：每个模块必须至少有一个可归属的 CTest 或 smoke test。只有编译覆盖、没有行为测试的模块，应记录为覆盖缺口，而不是标记为已完成。

## 2. 当前基线与缺口

### 2.1 已有能力

- 根 CMake 已启用 C++20、CTest 和 Debug/Release 输出。
- Runtime 包含 Core、Profiler、Jobs、Asset、Platform、RHI、Render、Resource、Input、Time、Physics、Framework、Scripts、Project 共 14 个模块。
- 默认构建 Game、Editor、Benchmark；测试目录当前注册 Core、Physics、Asset、Render、Profiler、Jobs、Project、Editor helper、Benchmark、Scene integration 和 Runtime boundary 等测试。
- 已有 `physics`、`stress`、`smoke` 等少量标签，并提供 `CHIKA_ENABLE_RUNTIME_SMOKE_TESTS` 开关。
- 第三方库以 Git submodule 为主，Python/codegen 依赖由 `uv.lock` 管理。

### 2.2 必须先解决的缺口

| 缺口 | 风险 | 计划动作 |
| --- | --- | --- |
| 仓库没有 `.github/workflows` | 没有远端自动门禁 | 新建 required CI 与 scheduled CI |
| 没有 `CMakePresets.json` | 本地和 CI 参数容易漂移 | 增加共享 configure/build/test presets |
| Git 中是 `engine/editor`，CMake/tests 引用 `engine/Editor` | 大小写敏感的 Linux checkout 会找不到目录/源码 | 统一目录名及全部引用的大小写 |
| Framework 使用 `CmakeLists.txt` | Linux 上 `add_subdirectory(Framework)` 不会找到标准文件名 | 重命名为 `CMakeLists.txt` |
| CTest 标签不完整 | 无法按模块证明覆盖 | 为测试补充 `unit`、`integration`、`module-*`、`smoke`、`stress` 标签 |
| Platform、RHI、Resource、Input、Time、Scripts 缺少清晰的独立行为门禁 | “所有模块通过”无法审计 | 增加最小 contract/lifecycle test，RHI 另加 smoke |
| 图形测试需要窗口、Vulkan loader/ICD | hosted runner 上可能配置成功但运行失败 | 把 headless 与 graphics smoke 分层，并显式安装软件 ICD/窗口环境 |
| CMake 配置阶段无条件执行 `uv sync` | 离线性、耗时和失败原因不清晰 | 改为 locked bootstrap；CI 先准备环境，CMake 只验证或受选项控制 |
| `pyproject.toml` 要求 Python 3.14 和 libclang 21 | runner 默认工具链不一定满足 | 使用固定版本的 `uv` 安装锁定 Python，并增加版本诊断 |
| 根 CMake 无条件定义 `JPH_USE_SSE4_1` | ARM/macOS 可移植性需确认 | 按架构条件化，并用 Apple Silicon job 验证 |
| 没有跨平台成功记录 | 不能证明 Windows/macOS/Linux 都可用 | 首次启用前保存三平台完整日志并修复根因 |

## 3. CI 总体结构

建议最终形成下列文件：

```text
.github/workflows/
  ci.yml                 # main PR、main push、手动触发；必需门禁
CMakePresets.json        # 本地与 CI 共用的配置/构建/测试入口
docs/test/
  github-actions-ci-plan.md
  main-branch-protection-guide.md
```

当前保持单一 workflow，不拆 reusable workflow。

### 3.1 触发器

`ci.yml`：

- `pull_request`：目标为 `main`，用于合并前门禁。
- `push`：分支为 `main`，覆盖 merge commit 和直接进入 main 的提交。
- `workflow_dispatch`：用于人工复现。
- 使用 `concurrency`，同一 PR 新提交到达时取消旧运行；每个 main commit 使用独立 SHA，确保不会互相取消。

不要使用 `pull_request_target` 执行来自 PR 的构建脚本，避免让不可信代码获得高权限上下文。

### 3.2 必需 job

1. `policy`：检查格式、文档、submodule 状态、CMake 配置约束和测试标签完整性。
2. `build-test`：三平台矩阵，配置、构建所有目标、运行 headless CTest。
3. `required`：使用 `needs` 汇总必需 job，形成稳定的分支保护检查名。
4. `graphics-smoke`：可选后续范围，当前 workflow 不执行。
5. `sanitizers`：可选手动深度检查，当前 workflow 不执行。
6. `release`：可选手动 Release 检查，当前 workflow 不执行。

矩阵使用 `fail-fast: false`，确保一个平台失败时其他平台仍给出诊断；这不改变最终必须全部成功的门禁。

## 4. 跨平台矩阵

实施时固定 runner 镜像，不使用会随时间漂移的 `*-latest`。首版建议如下；升级镜像要单独提交并跑完整矩阵。

| 层级 | Runner | 编译器/生成器 | 配置 | 必须构建 | 必须测试 |
| --- | --- | --- | --- | --- | --- |
| PR/main push | `ubuntu-24.04` x64 | GCC + Ninja | Debug | Runtime 14 模块、Engine、Game、Editor、Benchmark、tests | 全部现有 CTest，runtime smoke 关闭 |
| PR/main push | `windows-2022` x64 | MSVC 2022 | Debug | 同上 | 同上 |
| PR/main push | `macos-15` arm64 | AppleClang + Ninja | Debug | 同上 | 同上 |
| Smoke | 三个平台 | 与 PR 相同 | Debug | Game、Editor、RHI/Render 测试 | Game smoke；RHI 最小设备/提交；Editor 启停 |

如果 GitHub-hosted runner 的标签或架构可用性变化，应以 GitHub 官方 runner 列表为准更新固定标签。macOS ARM 与 Windows/MSVC 是不可被 Linux job 替代的必需门禁。

### 4.1 为什么 PR 仍要构建所有模块

通过 CTest 标签减少的是运行时间，不应通过关闭 Editor、Game、Benchmark 或某个 Runtime 子模块减少编译覆盖。跨平台最常见的问题来自条件编译、系统 SDK、链接器和动态库部署，只有完整构建才能及时暴露。

## 5. 模块门禁映射

| 模块/产品 | 当前可归属门禁 | 需新增或强化 |
| --- | --- | --- |
| Core | `Chika.CoreBoundary` | 增加 `module-core` 标签 |
| Profiler | `Chika.Profiler`、Timeline model | 增加 disabled/enabled 两种配置覆盖 |
| Jobs | `Chika.JobSystem`、`Chika.JobStress` | 两者都在三平台 CI 运行 |
| Asset | Asset pipeline、texture、environment、shader、asset/jobs | 补充损坏输入与路径大小写跨平台用例 |
| Platform | 由完整链接间接覆盖 | 新增 window contract；真实窗口放 smoke |
| RHI | Render tests 间接覆盖 | 新增 backend factory/capability test 和软件 Vulkan smoke |
| Render | baseline、phase 3/4、environment、graph、material | 保持 CPU contract 三平台；GPU 路径放 smoke |
| Resource | Asset/Render 间接覆盖 | 新增 handle 生命周期与异步 request test |
| Input | 由 Editor/Game 间接覆盖 | 新增 key map/backend contract，无窗口也可跑 |
| Time | 由 Scene 间接覆盖 | 新增 deterministic fake backend test |
| Physics | contract、lifecycle、contact、broadcast、authoring、motion | 已较完整；继续保持 `module-physics` 标签 |
| Framework | Core boundary、Scene integration、Physics integration | 增加 `module-framework` 标签 |
| Scripts | Physics broadcast 间接覆盖 | 新增 embedded Python init/import/error boundary test |
| Project | `Chika.ProjectDescriptor` | 增加 Windows 路径与 Unicode 路径用例 |
| Editor | Hierarchy、Timeline model | 增加无 UI model test；Editor 启停放 smoke |
| Game | 编译和可选 Game smoke | 三平台执行固定帧数 smoke |
| Benchmark | Benchmark unit test | Debug CI 必跑；性能阈值不属于当前门禁 |

任何新增 Runtime 模块都必须同时更新此映射、CTest label 和 CI 目标清单，否则 `policy` job 失败。

## 6. 测试分层与命令契约

### Tier 0：配置与完整构建，PR 必需

推荐由 preset 封装以下等价命令：

```sh
cmake -S . -B build/ci-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCHIKA_BUILD_GAME=ON \
  -DCHIKA_BUILD_EDITOR=ON \
  -DCHIKA_BUILD_TOOLS=ON \
  -DCHIKA_BUILD_BENCHMARKS=ON \
  -DCHIKA_ENABLE_RUNTIME_SMOKE_TESTS=OFF
cmake --build build/ci-debug --parallel
```

Windows PowerShell 使用相同 CMake 参数，但不要依赖 shell 专有的续行语法；workflow 中优先调用 preset。

### Tier 1：无头测试，PR 必需

```sh
ctest --test-dir build/ci-debug \
  --output-on-failure \
  --no-tests=error \
  -LE "smoke|stress" \
  --output-junit build/ci-debug/Testing/ctest.xml
```

- 为普通测试设置合理 timeout，不能无限等待。
- 不默认重试失败测试；先按失败处理并修复 flaky 根因。
- 测试必须从仓库根目录或明确工作目录运行，不依赖用户机器状态。

### Tier 2：图形与进程 smoke

- Linux：Xvfb + Mesa/Lavapipe Vulkan ICD，验证无实体 GPU 的可重复运行。
- Windows：安装 Vulkan runtime/software ICD；验证 Python DLL 和运行时 DLL 部署。
- macOS：安装 Vulkan SDK/MoltenVK，验证 Apple Silicon、窗口创建和 capability gate。
- Game：加载固定的最小 project/scene，运行固定帧数并正常退出。
- Editor：启动、创建主窗口、完成最小初始化后受控退出；保留日志与崩溃信息。
- RHI：创建 instance/device、提交最小 command、等待完成并销毁；能力缺失必须报告为明确 skip 条件，不能静默成功。

图形 smoke 当前不实现；未来若加入，应先作为手动观察项，稳定后再决定是否升为 PR 必需门禁。

### Tier 3：可选手动深度验证

- 三平台 Release 完整构建与全部 headless/stress 测试。
- Linux Clang ASan + UBSan；先处理一方代码问题，再评估第三方抑制清单。
- Benchmark 产出结构化结果作为 artifact；建立稳定基线后再设计阈值。
- 可选重复运行并发/Job/Physics 测试以寻找概率性问题，但不得用重试掩盖确定性失败。

这些内容不在当前 workflow 中，也没有定时触发器。

## 7. 环境与依赖准备

### 7.1 Checkout

- 必须递归拉取 submodule，并验证 submodule commit 与仓库记录一致。
- 对 fork PR 使用只读 token。
- Checkout、cache、artifact 等 action 在实际 YAML 中固定到完整 commit SHA；由依赖更新机器人或专门维护提交升级。

### 7.2 通用工具

- CMake 3.24 或更高版本。
- Ninja。
- 固定版本 `uv`，执行 locked sync；输出 `uv`、Python、CMake、Ninja 和编译器版本。
- Vulkan SDK/loader、平台窗口开发包和软件 ICD。
- 不把 runner 预装软件视为永久契约，所需版本必须在日志中可见。

### 7.3 平台注意事项

Linux 需要显式安装 GLFW 构建所需的 X11/Wayland 开发包、Vulkan loader/dev package、Mesa Vulkan driver 和 Xvfb。Windows 使用 MSVC 环境，检查 Vulkan 与 Python runtime DLL 能被构建产物找到。macOS 重点验证 MoltenVK 搜索路径、Apple Silicon 架构和不支持 GPU timestamp 的 queue capability 路径。

### 7.4 缓存

- 首选缓存 `uv` 下载缓存和编译器缓存；不要缓存 `.venv` 或整个 build tree 作为正确性的前提。
- key 至少包含 OS、架构、编译器、配置、`uv.lock`、CMake 文件和 submodule 状态摘要。
- 各 OS 缓存隔离；任何 cache miss 都必须能够从零完成构建。
- 缓存中不得包含 token、证书或其他秘密；恢复的缓存按不可信输入对待。

## 8. 日志、产物和失败诊断

每个 job 无论成功或失败都应提供清晰摘要：

- configure/build/test 各阶段耗时。
- 编译器、SDK、Vulkan、Python/uv 版本。
- CTest JUnit 报告。
- 失败时上传 `CMakeConfigureLog.yaml`、`CMakeCache.txt`、`Testing/Temporary/LastTest.log`、Game/Editor 日志和 crash dump（若存在）。
- 可选手动深度检查可上传 Benchmark JSON；普通 PR 不长期保存大型二进制。
- artifact 名称必须包含 OS、架构、编译器和配置，保留期以诊断所需的最短时间为准。

## 9. 权限与供应链约束

- workflow 顶层使用 `permissions: contents: read`，没有发布任务时不授予写权限。
- PR CI 不使用 secrets，不执行 `pull_request_target` 下的 PR 代码。
- 第三方 action 固定完整 SHA，并在注释中标记对应 release 版本。
- 依赖缓存不能包含秘密，也不能成为唯一依赖来源。
- 发布、签名、上传 release 不属于本次 CI 范围，未来应使用独立 workflow 和受保护 environment。

## 10. 分阶段安排

每个阶段用独立小提交完成；前一阶段通过本地验证后再进入下一阶段。

| 阶段 | 主要工作 | 交付物 | 完成条件 |
| --- | --- | --- | --- |
| P0 基线盘点 | 固化模块/目标/测试清单，记录三个平台依赖 | 本文档、基线清单 | 所有模块都有 owner 和预期门禁 |
| P1 统一入口 | 修复 Editor/Framework 路径大小写；增加 CMake presets；完善 CTest labels/timeout；locked Python bootstrap | 路径修复、`CMakePresets.json`、CMake/CTest 调整 | 大小写敏感 checkout 和本地 Debug 全构建、headless CTest 全通过 |
| P2 Linux CI | 落地 policy、Ubuntu Debug 全构建和 headless test | `.github/workflows/ci.yml` 初版 | 干净 runner 连续通过，失败 artifact 可用 |
| P3 Windows/macOS | 扩展 MSVC 与 AppleClang，修复平台根因 | 三平台 required matrix | 三个平台无允许失败项，全部完整构建和 headless test 通过 |
| P4 覆盖闭环 | 为 Platform/RHI/Resource/Input/Time/Scripts 补 contract tests；落地图形 smoke | 新测试、smoke fixture | 模块映射无“仅间接覆盖”，三平台 smoke 稳定 |
| P5 治理 | main branch protection、维护说明 | Ruleset、保护操作指南 | `CI Required` 固定、无 bypass、维护责任清晰 |

建议顺序是 P1 → P2 → P3 → P4 → P5。不要在 P2 只验证最小子集后就开启“所有模块通过”的分支保护描述；名称必须反映真实覆盖。

## 11. 分支与合并安排

1. 所有变更从功能分支通过 Pull Request 进入 `main`。
2. PR targeting `main` 必须运行三平台 matrix，并由 `CI Required` 汇总结果。
3. 按 `docs/test/main-branch-protection-guide.md` 创建 main Ruleset，要求 PR、最新分支和 `CI Required`。
4. 不设置 bypass，禁止直接 push、强推和删除 `main`。
5. PR 合并后由 `push` 事件在新的 main commit 上再次运行完整 CI。

## 12. 最终验收清单

- [ ] Linux、Windows、macOS 都从干净 checkout（含 submodules）成功配置。
- [ ] 三个平台都完整构建 14 个 Runtime 模块及 Engine/Game/Editor/Benchmark/tests。
- [ ] 三个平台 headless CTest 均为 100% 通过，且 `--no-tests=error` 生效。
- [ ] 每个一方模块都有可审计的 test/smoke 归属，不存在只靠链接推断的覆盖。
- [ ] Game、Editor、RHI smoke 在三个目标平台稳定运行。
- [ ] Main Ruleset 要求 `CI Required`，且直接 push/强推/删除均被拒绝。
- [ ] 失败日志和 JUnit/artifact 足以在本地复现。
- [ ] workflow 最小权限、action SHA 固定、PR 不使用 secrets。
- [ ] `docs/develop.md` 记录每个落地阶段的变更、原因、验证和剩余工作。
- [ ] GitHub 分支保护以稳定汇总 job 为 required check。

## 13. 实施时参考

- GitHub Actions matrix：<https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/run-job-variations>
- GitHub-hosted runners：<https://docs.github.com/en/actions/reference/runners/github-hosted-runners>
- Workflow syntax：<https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax>
- Dependency caching and security：<https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching>

这些链接用于实施时核对 runner 标签和语法；固定 runner/action 版本升级仍需通过完整矩阵验证。
