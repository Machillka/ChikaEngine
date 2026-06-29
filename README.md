# ChikaEngine

## 项目定位

ChikaEngine 是一个 C++ 自研引擎、引擎学习和工程化项目。它的重点不是马上做一个完整商业化引擎，而是围绕引擎架构、资源系统、运行时、编辑器、JobSystem、Profiling、Physics、Renderer、RHI 和工具链，建立可理解、可扩展、可验证的工程基础。

这个项目也服务于长期职业规划：互联网 / 游戏研发方向，尤其是 C++、游戏引擎、工具链、工程架构、性能分析和 Agent 辅助开发能力。代码和文档都应逐步变成适合 AI Agent 协作的形态：模块边界清晰、契约明确、验证路径明确、文档和代码同步演进。

## 当前阶段

### 已经存在并可以确认的能力

- CMake 根工程，C++20，默认构建 `ChikaEngine`、`ChikaRuntime`、`ChikaEditor`、`ChikaGame`、`ChikaBenchmark` 和测试。
- `Application` / `EngineContext` 统一管理主循环、窗口、时间、输入、脚本、JobSystem、AssetManager、Renderer、SceneManager 的生命周期。
- `ChikaEditor` 独立于 runtime 模块，基于 ImGui 和 Vulkan adapter 提供 viewport、inspector、hierarchy、log、render statistics、profiler timeline 等面板。
- `ChikaGame` 作为 standalone runtime 入口，不链接 Editor/ImGui，支持 `--project`、`--mode development|packaged`、`--smoke-frames`。
- `ChikaProject.json` 作为样例 project descriptor，包含 `contentRoot`、`cookedContentRoot`、startup scene GUID、窗口和 runtime 配置。
- `Framework` 层已有 `SceneManager`、`Scene`、`GameObject`、`Component`、`Transform`、`MeshRenderer`、`CameraComponent`、`LightComponent`、`Animator`、`Rigidbody`、Prefab、EventBus、Play/Edit 模式和固定步长物理同步。
- `Render` 层已有 `Renderer` facade、`RenderWorld` / `RenderWorldSnapshot`、`RenderPipeline`、`RenderGraph`、forward/deferred、parallel CPU preparation、GPU-driven visibility/indirect draw 的当前实现。
- `RHI` 层已有后端无关接口和 Vulkan backend，包含 buffer/texture/pipeline/command list、resource state、barrier、debug name、frame statistics、GPU timing、capability/fallback。
- `Asset` / `Resource` 层已有 GUID/meta `AssetDatabase`、`AssetReference`、`ImporterRegistry`、shader importer、scene importer、load/cache/reload/unload、async load，以及 CPU asset 到 GPU resource 的 `ResourceManager`。
- `Jobs` 层已有 engine-owned `JobSystem`，支持 worker、dependency、child job、wait-help、main-thread job、shutdown policy、statistics、`ParallelFor` 和 profiler integration。
- `Profiler` 层已有 RAII scope、frame、counter、instant、thread registry、history、aggregation、Perfetto trace export 和 editor timeline model。
- `Physics` 层已有 `PhysicsScene`、`IPhysicsBackend`、Jolt backend、body handle、rigidbody create/destroy、velocity、impulse、raycast、scene transform sync。
- `Benchmark` 入口支持 deterministic benchmark scenes、serial/jobs/gpu mode、worker count、warmup/sample frames、JSON output。
- 测试入口包括 CoreBoundary、SceneIntegration、AssetPipeline、AssetJobs、RenderBaseline、RenderPhase3、RenderPhase4、ShaderInterface、ProjectDescriptor、Profiler、JobSystem、JobStress、Benchmark 和 RuntimeBoundary。

### 正在建设中的能力

- `feat/render` 分支上的 GPU-driven renderer、parallel CPU renderer、render benchmark 和 diagnostics 仍在快速演进。
- Framework 作为 gameplay 和 engine service 的桥接层，目前耦合较重，尤其公开头文件会传递 Render/Resource/Physics/Asset 依赖。
- Runtime boundary check 已禁止 Runtime/Game 引用 Editor/ImGui，但还没有完整检查 Framework、RHI、Vulkan raw getter、Resource/Render 反向依赖等更细粒度边界。
- Development Game 仍允许 source asset scan/import/hot reload；Packaged Game 的 cooked source-free 路径仍未完整落地。
- Asset dependency graph、Cook Manifest、Cooked Asset Registry、Package Staging 是 runtime standalone roadmap 的后续阶段。
- Physics collision events、transform sync 性能、layer details、gameplay query adapter 仍需要继续补齐。

### 规划中但尚未完成的能力

- Source-free standalone package：不依赖 `Assets/` 源文件、`.meta`、shader 源码或 importer。
- Cook pipeline：显式 root assets、传递依赖闭包、type-specific cook rules、cooked content layout、runtime cooked provider。
- 更细的 module contract 文档和 AI Agent playbook。
- Renderer Phase 5：Hi-Z occlusion、clustered lighting、secondary command recording、transient memory aliasing、async compute 等按 benchmark 数据选择，不应一次性全做。
- 更完整的 CI/regression gate、benchmark report automation、portfolio artifacts。
- TODO / 待确认：跨平台构建矩阵、正式分支/commit 规范、完整文档索引、headless runtime 在无 GPU 环境下的测试策略。

## 快速开始

### 环境要求

已确认要求：

- Git，并初始化 submodule。
- CMake `>= 3.24`。
- C++20 编译器。Windows 推荐 Visual Studio 2022 / MSVC v143 或可用的 Clang toolchain。
- `uv`。根 CMake 配置阶段会执行 `uv sync`。
- Python 环境满足 `pyproject.toml`：`requires-python = ">=3.14"`，并需要 `Python3 Development.Embed`。
- Vulkan SDK。`engine/ThirdParty/CMakeLists.txt` 使用 `find_package(Vulkan REQUIRED)`。
- OpenGL 开发环境。当前第三方配置中仍有 OpenGL 查找需求，详见 `docs/build-after-pull.md`。
- Git submodule 中的第三方库：`glfw`、`imgui`、`fmt`、`nlohmann/json`、`tinyobjloader`、`stb`、`JoltPhysics`、`pybind11`、`VulkanMemoryAllocator`、`tinygltf`、`SPIRV-Reflect` 等。

TODO / 待确认：

- Linux/macOS 的完整依赖安装命令。
- Python 3.14 在不同开发机上的可获取性和 fallback 策略。
- 不同 GPU/driver 的 Vulkan validation 运行基线。

### 拉取仓库

```powershell
git clone --recursive <repo-url>
cd ChikaEngine
```

如果已 clone 但第三方库缺失：

```powershell
git submodule update --init --recursive
```

### 配置 / 生成工程

推荐 out-of-source build：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Visual Studio 生成器示例：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

常用构建选项：

```text
CHIKA_BUILD_GAME=ON
CHIKA_BUILD_EDITOR=ON
CHIKA_BUILD_TOOLS=ON
CHIKA_BUILD_BENCHMARKS=ON
CHIKA_ENABLE_PROFILING=ON
CHIKA_ENABLE_RUNTIME_SMOKE_TESTS=OFF
BUILD_TESTING=ON
```

注意：配置阶段会执行 `uv sync`，并生成反射代码 `ReflectionRegistry.cpp` / `PythonRegistry.cpp`。

### 编译

```powershell
cmake --build build
```

多配置生成器：

```powershell
cmake --build build --config Debug
```

### 运行

Editor：

```powershell
.\build\bin\ChikaEditor.exe
```

Game development runtime：

```powershell
.\build\bin\ChikaGame.exe --project ChikaProject.json
```

Game smoke：

```powershell
.\build\bin\ChikaGame.exe --project ChikaProject.json --smoke-frames 3
```

Benchmark help：

```powershell
.\build\bin\ChikaBenchmark.exe --help
```

Benchmark 示例：

```powershell
.\build\bin\ChikaBenchmark.exe --scene state-diversity --mode gpu --workers 1 --warmup 3 --frames 5 --output build\phase4-gpu-state-diversity.json
```

### 测试

完整测试：

```powershell
ctest --test-dir build --output-on-failure
```

按名称运行：

```powershell
ctest --test-dir build -R Chika.RenderPhase4 --output-on-failure
ctest --test-dir build -R Chika.JobSystem --output-on-failure
ctest --test-dir build -R Chika.RuntimeBoundary --output-on-failure
```

窗口化 game smoke 作为 CTest 需要配置：

```powershell
cmake -S . -B build -DCHIKA_ENABLE_RUNTIME_SMOKE_TESTS=ON
```

TODO / 待确认：是否需要区分默认快速测试、GPU validation 测试、stress 测试和 release benchmark gate。

### 常见调试入口

- Engine lifecycle：`engine/include/ChikaEngine/Application.hpp`、`engine/include/ChikaEngine/EngineContext.hpp`、`engine/src/Application.cpp`、`engine/src/EngineContext.cpp`。
- Editor 启动：`engine/Editor/src/main.cpp`、`engine/Editor/src/EditorManager.cpp`。
- Game 启动：`engine/Game/src/main.cpp`、`ChikaProject.json`。
- Render frame：`engine/Runtime/Render/include/ChikaEngine/Renderer.hpp`、`RenderPipeline.hpp`、`RenderGraph.hpp`。
- Gameplay bridge：`engine/Runtime/Framework/include/ChikaEngine/subsystem/RenderSubsystem.h`、`scene/scene.hpp`。
- Asset load/reload：`engine/Runtime/Asset/include/ChikaEngine/AssetManager.hpp`、`AssetDatabase.hpp`、`AssetImporter.hpp`。
- Jobs：`engine/Runtime/Jobs/include/ChikaEngine/jobs/JobSystem.hpp`。
- Physics：`engine/Runtime/Physics/include/ChikaEngine/PhysicsScene.h`、`PhysicsDescs.h`、`component/Rigidbody.hpp`。

## 仓库结构

```text
.
├── Assets/                  # 示例源资产、shader、materials、scenes、scripts 和 .meta
├── docs/                    # 架构、路线图、修复记录、benchmark/profiler/jobs/render 结果
├── engine/
│   ├── include/             # Engine facade: Application, EngineContext
│   ├── src/                 # Engine facade 实现
│   ├── Runtime/             # runtime 模块集合
│   │   ├── Core/            # math/debug/io/reflection/serialization/base
│   │   ├── Profiler/        # CPU/GPU profiler 数据与导出
│   │   ├── Jobs/            # JobSystem, queues, storage, ParallelFor
│   │   ├── Asset/           # AssetDatabase, Importer, loaders, AssetReference
│   │   ├── Resource/        # CPU asset 到 GPU resource adapter
│   │   ├── RHI/             # RHI 接口和 Vulkan backend
│   │   ├── Render/          # Renderer, RenderWorld, RenderPipeline, RenderGraph
│   │   ├── Framework/       # Scene/GameObject/Component/subsystems
│   │   ├── Physics/         # PhysicsScene, Jolt backend, descriptors
│   │   ├── Platform/        # Window abstraction, GLFW backend
│   │   ├── Input/           # InputSystem, GLFW input backend
│   │   ├── Time/            # TimeSystem, GLFW time backend
│   │   ├── Scripts/         # Python/pybind scripting system
│   │   └── Project/         # ProjectDescriptor, RuntimeMode, boot config
│   ├── Editor/              # ChikaEditor executable and editor-only panels/adapters
│   ├── Game/                # ChikaGame standalone runtime executable
│   ├── Benchmark/           # ChikaBenchmark and benchmark helper executables
│   ├── tools/reflector/     # Python reflection/codegen tool
│   ├── cmake/               # engine CMake helpers
│   └── ThirdParty/          # vendored/submodule dependencies
├── tests/                   # unit/integration/stress tests and RuntimeBoundaryCheck
├── CMakeLists.txt           # root build entry
├── ChikaProject.json        # sample project descriptor
├── pyproject.toml           # uv-managed Python tooling environment
├── uv.lock                  # Python dependency lock
├── .clang-format            # C++ formatting rules
└── AGENTS.md                # Agent / Codex repository instructions
```

不要修改 `engine/ThirdParty/`，除非任务明确要求更新第三方依赖。

## 模块信息

### Core / Foundation

- 职责：基础 math、handle/slot map、UID、fixed step accumulator、logging、assert、gizmo debug data、IO stream、reflection、serialization、shader interface 基础设施。
- 位置：`engine/Runtime/Core/`。
- 关键类型 / 文件：`math/*`、`debug/log_system.h`、`debug/assert.h`、`base/UIDGenerator.h`、`base/SlotMap.h`、`io/IStream.h`、`reflection/TypeRegister.h`、`reflection/ReflectionMacros.h`。
- 当前状态：已作为多数 runtime 模块的共同基础层；`ChikaCore` public link `ChikaThirdParty`。
- 边界：Core 不应反向依赖 Framework、Render、Asset、Editor；不要把高层业务逻辑放进 Core。
- 验证：`cmake --build build`，`ctest --test-dir build -R Chika.CoreBoundary --output-on-failure`。

### Runtime / Framework

- 职责：运行时主循环由 `Application` / `EngineContext` 驱动；Framework 提供 `Scene`、`SceneManager`、`GameObject`、`Component` 和 gameplay-facing subsystems。
- 位置：`engine/include/ChikaEngine/`、`engine/src/`、`engine/Runtime/Framework/`。
- 关键类型 / 文件：`Application`、`EngineContext`、`SceneManager`、`Scene`、`GameObject`、`Component`、`Transform`、`RenderSubsystem`、`PhysicsSubsystem`、`AnimationSubsystem`。
- 当前状态：Editor runtime 和 standalone Game runtime 已拆分入口；`ChikaGame` 不链接 Editor/ImGui；Framework 已有 Play/Edit 模式、延迟销毁、组件生命周期、Prefab、EventBus 和 scene serialization。
- 边界：`EngineContext` 是 composition root，可以高耦合；Framework 是 gameplay 到 Render/Physics/Asset 的桥，不应继续把 RHI/Vulkan/backend 细节暴露给 gameplay header。
- 验证：`Chika.SceneIntegration`、`Chika.RuntimeBoundary`、`Chika.GameSmoke`。

### Renderer

- 职责：消费 `RenderWorldSnapshot`，管理 render device context、resource system、pipeline、render graph、visibility、queue/batch、forward/deferred、post process、GPU-driven path 和 diagnostics。
- 位置：`engine/Runtime/Render/`。
- 关键类型 / 文件：`Renderer.hpp`、`RenderWorld.hpp`、`RenderPipeline.hpp`、`RenderGraph.hpp`、`RenderSettings.hpp`、`RenderPath.hpp`、`RenderPreparation.hpp`、`GpuDrivenData.hpp`。
- 当前状态：Renderer 不直接读取 `Scene/GameObject/Component`；通过 `RenderSubsystem -> RenderWorld -> RenderWorldSnapshot` 获取帧输入。当前分支已有 parallel CPU preparation 和 GPU-driven static opaque visibility/indirect draw。
- RHI/RGPU 概念：`IRHIDevice`、`IRHICommandList`、`RHIDesc`、buffer/texture/sampler/view/pipeline/shader/resource binding、resource state/barrier、debug name、GPU timing。
- Shader/material/texture/mesh 关系：Asset 负责 CPU 数据和导入，Resource 负责上传为 GPU 资源，RenderPipeline/RenderGraph 负责在 pass 中消费。
- 边界：RenderPipeline 不应 include 或读取 Framework；Renderer facade 不应成为 Editor UI backend；Gameplay 不应直接操作 RenderGraph。
- 验证：`Chika.RenderBaseline`、`Chika.RenderPhase3`、`Chika.RenderPhase4`、GPU smoke benchmark、必要时 Vulkan validation。

### RHI

- 职责：提供后端无关渲染硬件接口和当前 Vulkan backend。
- 位置：`engine/Runtime/RHI/`。
- 关键类型 / 文件：`IRHIDevice.hpp`、`IRHICommandList.hpp`、`RHIDesc.hpp`、`RHIResourceHandle.hpp`、`RHICapabilities.hpp`、`ResourceBinder.hpp`、`rhi/Vulkan/VulkanRHIDevice.hpp`。
- 当前状态：RHI 接口和 Vulkan 实现在同一 `ChikaRHI` target；命名空间仍在 `ChikaEngine::Render` 下。
- 边界：Runtime/gameplay 不应直接依赖 Vulkan raw handle。当前 Editor `VulkanAdapter` 可以使用 Vulkan backend 细节，这是 editor-only 适配层。
- 验证：render tests、editor launch smoke、GPU benchmark、Vulkan validation。TODO / 待确认：非 Vulkan backend 目标和 mock RHI 测试策略。

### Asset / Resource System

- 职责：Asset 层管理 source asset 身份、meta、导入、CPU 数据、cache、hot reload 和 async loading；Resource 层把 CPU asset 上传并缓存为 GPU resource。
- 位置：`engine/Runtime/Asset/`、`engine/Runtime/Resource/`。
- 关键类型 / 文件：`AssetDatabase`、`AssetGuid`、`AssetRecord`、`AssetReference`、`AssetManager`、`ImporterRegistry`、`IAssetImporter`、`ShaderImporter`、`SceneImporter`、`ResourceManager`、`ResourceHandle`、`ResourceLayout`。
- 已实现事实：
  - `AssetDatabase` 以 GUID/meta 建立 source asset identity，并按 path/GUID 查询。
  - `AssetReference` 保存 GUID、SubAsset、expected type 和 `diagnosticPath`。GUID 是稳定身份；`diagnosticPath` 仅用于开发态诊断和旧路径迁移。
  - `AssetManagerCreateInfo` 可控制 `scanAssets`、`createMissingMeta`、`importAssets`、`enableHotReload`，并可注入 `JobSystem`。
  - `AssetManager` 提供 texture/mesh/shader/material/shader template/animation clip 的 load/cache/unload/reload 和部分 async load。
  - `ImporterRegistry` 已有 passthrough、shader、scene importer。
  - `ResourceManager` 依赖 `IRHIDevice` 和 `AssetManager`，负责 mesh/texture/material upload，持有 CPU asset handle 到 GPU resource handle 的 cache，并订阅 asset reload。
- 设计目标 / 规划契约：
  - Asset handle 是进程内缓存句柄，不应持久化；持久引用使用 GUID/SubAsset。
  - CPU-side resource state 和 GPU upload state 要分开理解：AssetManager 只证明 CPU 数据可用，ResourceManager 才证明 GPU resource 可用。
  - load/cache/reload/unload 是 CPU asset 生命周期；GPU upload/unload 是 ResourceManager/RHI 生命周期。
  - Hot reload 预期由 AssetManager 发现变更并发布 reload event，ResourceManager 再刷新受影响 GPU resource。
  - Cooked Runtime 不应扫描源资产、不生成 meta、不运行 importer、不依赖 shader 源码；当前仍属后续规划。
  - 新增 asset type 时要先定义 source asset、runtime CPU data、importer output、handle/cache、reload/unload，再考虑 GPU upload。
- 边界：
  - Importer/tools 负责转换源资产，不应直接驱动 Renderer。
  - Runtime loader/registry 负责加载当前模式允许的内容，不应隐式回退到 editor-only 路径。
  - ResourceManager 是 Asset 到 RHI 的 adapter，Gameplay 不应直接依赖它。
- 验证：`Chika.AssetPipeline`、`Chika.AssetJobs`、`Chika.ShaderInterface`、`Chika.ProjectDescriptor`、editor/game smoke。Cooked asset 验证为 TODO。

### Jobs / Profiling

- 职责：JobSystem 提供 engine-owned CPU scheduler；Profiler 提供跨 engine/render/jobs 的 instrumentation、aggregation 和 timeline/trace 数据。
- 位置：`engine/Runtime/Jobs/`、`engine/Runtime/Profiler/`。
- 关键类型 / 文件：`JobSystem`、`JobHandle`、`JobDesc`、`JobState`、`JobTarget`、`JobFailurePolicy`、`JobShutdownPolicy`、`ParallelFor`、`ProfilerMacros.hpp`、`ProfilerScope.hpp`、`ProfilerSession.hpp`。
- 已实现事实：
  - `JobSystem::Initialize` 分配 bounded queues/storage 并启动 worker。
  - `JobSystem::Shutdown` 支持 `Drain` 和 `CancelPending`。
  - `Schedule` 返回显式拥有的 `JobHandle`。
  - `ScheduleAfter` 建立 dependency continuation。
  - `ScheduleChild` 让 parent completion counter 等待 child。
  - `Wait` 会让 scheduler worker/main thread 帮忙执行 ready work。
  - `Release` 回收 terminal handle，`Detach` 把 handle cleanup 交给 scheduler。
  - `PumpMainThreadJobs` 执行 `JobTarget::MainThread` 任务。
  - `ParallelFor` 会拆分 bounded chunks，并返回 join handle。
  - `CHIKA_PROFILE_SCOPE`、`CHIKA_PROFILE_FRAME`、`CHIKA_PROFILE_COUNTER`、`CHIKA_PROFILE_INSTANT` 受 `CHIKA_ENABLE_PROFILING` 控制。
- 契约草案：
  - Job owner 必须明确谁负责 wait/release/detach。
  - Parent/child 表示生命周期完成关系，不是严格执行顺序；执行顺序由 dependency/queue/target 决定。
  - Worker thread 不应调用 main-thread-only API、Editor UI、窗口 API 或共享 Vulkan command pool。
  - 捕获 engine object 到 job 中必须证明对象生命周期覆盖 job 运行；否则只捕获值、handle 或只读 snapshot。
  - Blocking wait 应集中在明确同步点，避免在热路径无意阻塞。
  - Frame-end synchronization 需要说明等待哪些 jobs，不能靠 shutdown drain 隐式兜底。
  - Long-running work 应添加 profiler scope，细粒度对象/绘制级 scope 要谨慎，避免 profiler overhead 失控。
- 验证：`Chika.JobSystem`、`Chika.JobStress`、`Chika.AssetJobs`、`Chika.Profiler`、`Chika.ProfilerTimelineModel`、`ChikaJobBenchmark`。

### Physics

- 职责：物理场景、body 生命周期、刚体组件、Jolt backend、固定步长 simulation 和 scene transform sync。
- 位置：`engine/Runtime/Physics/`、`engine/Runtime/Framework/src/subsystem/PhysicsSubsystem.cpp`、`engine/Runtime/Framework/include/ChikaEngine/component/Rigidbody.hpp`。
- 关键类型 / 文件：`PhysicsScene`、`IPhysicsBackend`、`PhysicsJoltBackend`、`PhysicsBodyHandle`、`PhysicsBodyCreateDesc`、`RaycastHit`、`Rigidbody`。
- 已实现事实：
  - `PhysicsScene` 持有 backend，并提供 `CreateBodyImmediate`、`EnqueueRigidbodyDestroy`、`SetLinearVelocity`、`ApplyImpulse`、`Raycast`、`SetBodyTransform`、`PollTransform`。
  - `Rigidbody` 持有 `PhysicsBodyHandle`，通过 Scene/PhysicsScene 创建和销毁 body。
  - Scene 使用 fixed step accumulator 驱动物理；PhysicsSubsystem 将 simulation 后的 transform 回写 scene。
- 规划契约：
  - Physics backend 不应泄露到 gameplay 层；gameplay 只通过 `Rigidbody`、Scene/Physics query adapter 或稳定 handle 访问。
  - Body handle ownership 应属于 PhysicsScene/Rigidbody 的组合，不应让任意 gameplay 代码长期持有裸 backend 对象。
  - Transform authority 要按阶段区分：simulation step 内 physics 是动态 body 的权威；editor edit/kinematic body 可由 scene transform 写入 physics。
  - Fixed-step simulation 和 variable-step frame update 必须保持分离。
  - Collision/query result lifetime 要明确，不能返回 backend 内部临时指针。
  - Gameplay physics query 应通过稳定 API 暴露，避免直接访问 Jolt。
- 当前缺口：collision events、layer details、transform sync 性能、默认 gravity 策略、query adapter 文档。
- 验证：`Chika.SceneIntegration`、未来 physics unit/integration tests。TODO / 待确认：独立 Physics 测试入口。

### Editor

- 职责：编辑器进程、ImGui UI、viewport、hierarchy、inspector、log、render statistics、profiler timeline、editor-only Vulkan UI adapter。
- 位置：`engine/Editor/`。
- 关键类型 / 文件：`EditorApplication` in `engine/Editor/src/main.cpp`、`EditorManager`、`EditorContext`、`ViewportPanel`、`InspectorPanel`、`SceneHierarchyPanel`、`RenderStatisticsPanel`、`ProfilerTimelinePanel`、`VulkanAdapter`。
- 当前状态：Editor 使用 `Application` / `EngineContext`，创建 baseline scene，并通过 overlay callback / adapter 接入 UI。Editor-only code 不应进入 Runtime/Game。
- 边界：Editor 是 runtime 服务的使用方，不应成为 runtime state 的第二所有者；不要让 ImGui 或 editor panels 污染 headless/game runtime。
- 验证：启动并关闭 `ChikaEditor`，运行 `Chika.RuntimeBoundary`。TODO / 待确认：自动化 editor smoke test。

### Tools / Importers / Cook Pipeline

- 已有工具：
  - `engine/tools/reflector/`：Python reflection/codegen，使用 `register.py`、Jinja templates、Clang Python tooling。
  - Asset importers：`PassthroughImporter`、`ShaderImporter`、`SceneImporter`。
  - Benchmark tools：`ChikaBenchmark`、`ChikaProfilerOverheadBenchmark`、`ChikaJobBenchmark`、`ChikaRenderCpuBenchmark`。
- 规划中：
  - Asset dependency graph。
  - Cook Manifest。
  - Type-specific cook rules。
  - Cooked Registry / Cooked Runtime Provider。
  - Package Staging / one-command package target。
- 边界：offline tools 可以读取 source assets 和 meta；packaged runtime 不应依赖这些开发态能力。
- 验证：reflection 由 CMake 配置/构建触发；asset pipeline 用 `Chika.AssetPipeline`；cook/package 验证为 TODO。

### Tests / Verification

- 单元测试：`tests/unit/`。
- 集成测试：`tests/integration/`。
- 压力测试：`tests/stress/JobStealingStressTests.cpp`。
- 边界测试：`tests/cmake/RuntimeBoundaryCheck.cmake`。
- Runtime smoke：`Chika.GameSmoke` 需要 `CHIKA_ENABLE_RUNTIME_SMOKE_TESTS=ON`。
- 当前缺口：
  - Physics 独立测试不足。
  - Cook/package/source-free runtime 验证尚未落地。
  - Editor 自动 smoke 尚未固化。
  - GPU validation / RenderDoc capture / benchmark release matrix 尚未形成统一 gate。
  - TODO / 待确认：默认 CI 是否存在，以及不同平台的测试子集。

## AI Agent 协作说明

### 工作原则

- 修改前先阅读相关模块、最近提交、`AGENTS.md` 和相关 docs。
- 小步修改，不做无请求的大重构。
- 不要随意移动目录或重命名核心类型。
- 不要修改 `engine/ThirdParty/`、vendored、external、submodule，除非明确要求。
- 不要打印、提交或复制密钥、token、cookie。
- 对不确定的 API、CLI、依赖版本，先查官方文档或项目内文档。
- 修改后尽量运行对应 build / test / format 命令。
- 结束前说明改了什么、为什么改、如何验证、剩余风险。
- 如果某能力无法从仓库确认，写「TODO / 待确认」，不要伪造成已实现。

### 修改入口判断

- 资源导入问题：先看 `engine/Runtime/Asset/`、`AssetImporter.hpp`、`AssetDatabase.hpp`、`engine/tools/reflector/` 或未来 cook tools。
- Runtime 加载问题：先看 `AssetManager`、`ProjectDescriptor`、`RuntimeBootConfig`、`SceneManager::LoadScene`。
- GPU 上传问题：先看 `ResourceManager`、`RenderResourceSystem`、`IRHIDevice` 和相关 RHI resource lifecycle。
- 热重载问题：先看 `AssetManager::TickHotReload`、reload subscription、`ResourceManager` reload 处理、Editor integration。
- Cooked asset 问题：先看 `docs/runtime/` roadmap；当前代码路径仍待补齐，不要假设已完成。
- Job 问题：先看 `JobSystem`、`JobStorage`、`JobQueue`、`ParallelFor`、scheduler statistics 和 shutdown policy。
- Profiler 问题：先看 `ProfilerMacros.hpp`、`ProfilerSession`、`ProfilerAggregator`、`ProfilerTimelineModel`。
- Physics transform 问题：先看 `PhysicsSubsystem`、`PhysicsScene::PollTransform`、`Rigidbody`、Scene fixed step。
- Editor 显示问题：先看 `EditorManager`、`EditorContext`、`ViewportPanel`、`InspectorPanel`、`VulkanAdapter`。
- Headless/runtime 问题：不要依赖 editor-only code、ImGui、Editor panels 或 Vulkan UI adapter。

### Agent 输出格式建议

每次完成任务建议输出：

```text
Summary
Changed files
Verification
Risks / TODO
Next actions
```

## 开发约定

- C++ 标准：C++20。
- 构建系统：CMake `>= 3.24`，推荐 out-of-source build。
- 格式化：遵循 `.clang-format`。当前基于 Microsoft style，4 空格，Allman braces，`ColumnLimit: 500`，`SortIncludes: false`。
- 命名空间：主要使用 `ChikaEngine::*`，例如 `ChikaEngine::Engine`、`Render`、`Asset`、`Resource`、`Framework`、`Physics`、`Jobs`、`Profiler`。
- 反射：使用 `MCLASS`、`REFLECTION_BODY`、`MFIELD`、`MFUNCTION`，通过 Python reflector 生成注册代码。
- 日志：使用 `LOG_DEBUG`、`LOG_INFO`、`LOG_WARN`、`LOG_ERROR`、`LOG_FATAL`。
- 断言：使用 `CHIKA_ASSERT`，Debug/Release 行为见 `debug/assert.h`。
- Profiling：使用 `CHIKA_PROFILE_SCOPE`、`CHIKA_PROFILE_FUNCTION`、`CHIKA_PROFILE_FRAME`、`CHIKA_PROFILE_COUNTER`、`CHIKA_PROFILE_INSTANT`。
- Ownership / lifetime：优先 RAII、`std::unique_ptr`、明确 owner。Handle 需要配合 generation/validity 语义，不要跨生命周期缓存裸指针。
- Include 约定：TODO / 待确认。当前已知风险是 Framework 公开头文件包含 Render/Resource/Physics/Asset 过多。
- Error handling：启动失败和 project/runtime boot config 使用 bool/error string 或返回码；部分 runtime 异常由 `Application::Run` 捕获并记录日志。
- Commit / branch 约定：TODO / 待确认。现有历史使用 `feat/...` 分支和 PR merge，但没有在仓库内找到正式规范。

## 添加新模块的建议流程

1. 明确模块边界：职责、非职责、允许依赖、禁止依赖。
2. 新增源码目录：遵循 `include/ChikaEngine/...` 和 `src/` 结构。
3. 接入构建系统：新增模块 `CMakeLists.txt`，只链接真实依赖，不依赖 transitive dependency 偷拿 header。
4. 添加最小 runtime/editor 入口：只在 composition root 或明确 bridge 中接入。
5. 添加测试或 smoke test：至少覆盖 lifecycle、失败路径和边界规则。
6. 更新 README / docs：记录入口文件、验证命令和限制。
7. 记录验证命令：`cmake --build build`、相关 `ctest -R ...`、必要时 editor/game/benchmark smoke。

## 添加新 Asset Type / Importer 的建议流程

1. 定义 source asset 与 runtime CPU resource。
2. 定义 importer 输入输出，包括 imported path、sidecar、错误信息和可重复性。
3. 注册 asset type：更新 classify、type name、default importer 和 registry。
4. 处理 path / handle 稳定性：持久引用使用 GUID/SubAsset，runtime handle 只在进程内使用。
5. 处理 cache / reload / unload：定义缓存 key、reload event、失效策略和 unload 行为。
6. 如涉及 GPU，定义 upload 时机：AssetManager 不直接创建 GPU resource，交给 ResourceManager/RHI。
7. 增加 editor 验证：asset browser/inspector/viewport 路径，TODO / 待确认具体入口。
8. 增加 headless runtime 验证：Development Game 不依赖 editor-only code。
9. 增加 cooked asset 验证：TODO / 待确认，当前 Cooked Registry/Provider 尚未落地。

## 添加新 Job / 后台任务的建议流程

1. 明确 job owner：谁持有 `JobHandle`，谁负责 wait/release/detach。
2. 明确生命周期：任务是否可跨帧、是否依赖 scene/resource/renderer 当前帧状态。
3. 明确是否允许取消：选择 `JobFailurePolicy` 和 shutdown policy。
4. 明确是否可阻塞等待：避免在 render/gameplay 热路径意外 wait。
5. 明确捕获数据是否线程安全：优先捕获值、只读 snapshot、稳定 handle。
6. 明确 main-thread-only API：窗口、Editor UI、部分 RHI/Vulkan、Scene mutation 默认视为 main-thread-only，除非有明确文档。
7. 添加 profiler scope：长任务和阶段性任务必须可观测。
8. 添加 shutdown 验证：至少覆盖 drain/cancel、dependency、child job 和 stale handle。

## Roadmap

### Near-term

- 把 README、`AGENTS.md`、runtime/render/asset/jobs docs 串成清晰 docs index。
- 为 `EngineContext`、`Framework`、`Render`、`Asset/Resource`、`Jobs`、`Physics` 添加模块契约。
- 收敛 Framework 公开头文件耦合，至少先文档化允许依赖和禁止依赖。
- 补齐 RuntimeBoundaryCheck 的细粒度规则：Editor/ImGui 之外，继续检查 Vulkan raw getter、Framework/RHI 泄露、Resource 反向依赖。
- 保持测试可运行：CoreBoundary、SceneIntegration、AssetPipeline、RenderPhase、JobSystem、Profiler、RuntimeBoundary。

### Mid-term

- Asset pipeline：依赖图、Cook Manifest、Cooked Registry、Cooked Runtime Provider。
- JobSystem：更完整的 shutdown/race/stress 覆盖，和真实 Asset/Render 工作负载回归。
- Profiling：保持 overhead gate，完善 editor timeline 和 trace 输出样例。
- Physics：明确 transform authority、collision/query lifetime、独立 physics tests。
- Editor/Runtime 分离：确保 `ChikaGame` 和 headless/benchmark 路径不依赖 editor-only code。

### Long-term

- Renderer 扩展：基于 Release benchmark 数据选择 Hi-Z occlusion、clustered lighting、secondary command recording、transient memory aliasing 或 async compute。
- Cook Pipeline / Package：source-free standalone runtime，仓库外可复制运行。
- 工具链：资产导入、cook、package、benchmark report automation、regression gates。
- AI Agent 深度协作：module contract、task playbook、verification matrix、边界检查和稳定失败诊断。

## Known Issues / TODO

- 构建命令的跨平台环境准备待补齐，尤其 Python 3.14、Vulkan SDK、OpenGL dev packages。

- 测试入口需要分层：quick、GPU、stress、benchmark、editor smoke。

- Asset lifecycle contract 已有基础实现，但 cooked/runtime source-free contract 尚未落地。

- JobSystem contract 已有实现和测试，仍需更系统的 race/shutdown 说明和真实负载回归。

- Physics transform sync 当前存在低效 FIXME，collision events 和 layer details 仍是 TODO。

- Editor 与 Runtime 边界已有 RuntimeBoundaryCheck，但需要覆盖更多 include/dependency 规则。

- `Framework` 公开头文件耦合 Render/Resource/Physics/Asset 较多，短期可接受，但不应继续扩散。

- Reflection tooling 有若干 TODO：容器递归清理、绝对路径、error check 机制。

- Input callback、Window factory、assert popup、log API string_view 等基础设施 TODO 仍未处理。

- Release benchmark matrix 和 Phase 6 regression/portfolio 自动化仍待补齐。

  

## 文档维护规则

- README 是项目入口，不写过深实现细节。
- 深入设计、阶段记录、benchmark 数据和修复过程放到 `docs/` 下的专题文档。
- 每次模块边界、构建方式、运行入口、验证方式变化后更新 README。
- 不确定内容必须标记 `TODO / 待确认`，不允许伪造确定性。
- 新增模块、asset type、job system 行为、physics contract、cook/package 行为时，同步更新对应 docs 和 README 的索引信息。
