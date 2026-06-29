# Runtime Architecture Dependency Analysis

## Metadata

- Date: 2026-06-29
- Scope: `engine/`, `engine/Runtime/`, `engine/Game/`, `engine/Editor/`, `tests/`, existing design docs
- Excluded: `engine/ThirdParty/` implementation details
- Status: Current-state analysis for stabilizing existing runtime code

## Summary

当前工程已经从早期“入口直接串联所有系统”的形态，演进成一个比较清楚的小型引擎框架：

```text
ChikaGame / ChikaEditor / ChikaBenchmark / tests
    -> ChikaEngine
        -> ChikaRuntime
            -> Core / Profiler / Jobs / Asset / Platform / RHI / Resource / Render
            -> Framework / Input / Time / Physics / Scripts / Project
```

最重要的稳定化成果有三点：

- `Application` 和 `EngineContext` 已经成为主生命周期边界，负责初始化、主循环、逆序关闭。
- Renderer 已经从“Gameplay 直接提交 draw command”升级为 `RenderSubsystem -> RenderWorld -> RenderWorldSnapshot -> RenderPipeline -> RenderGraph -> RHI`。
- Scene/Gameplay 已经有基本可用的对象生命周期、延迟销毁、Play/Edit 快照、Prefab、事件和固定步长物理。

当前最大耦合点不在 Vulkan 或 RenderGraph，而在 `Framework` 层：Scene、Component、Subsystem 为了桥接 gameplay、render、asset、physics，公开头文件里已经直接暴露了 `Renderer`、`RenderWorld`、`ResourceManager`、`PhysicsScene`、`AssetManager` 等类型。这个桥接层可以先稳定，不需要大重构，但应该避免继续扩散。

## Current Frame Flow

当前主循环由 `Application::Run()` 驱动：

```text
Application::Run
    -> EngineContext::Initialize
    -> loop
        -> EngineContext::BeginFrame
            -> Window::Tick
            -> TimeSystem::Update
            -> InputSystem::Update
            -> AssetManager::TickHotReload
            -> JobSystem::PumpMainThreadJobs
        -> Application::OnUpdate
        -> EngineContext::Tick
            -> Renderer::BeginFrame
            -> SceneManager::Tick
                -> Scene::Tick
                    -> fixed GameObject::FixedTick
                    -> PhysicsSubsystem::Tick + SyncTransform
                    -> GameObject::Tick
                    -> AnimationSubsystem::Tick
                    -> GameObject::LateTick
                    -> RenderSubsystem::Tick
                        -> Sync Gameplay components
                        -> ResourceManager upload/resolve
                        -> RenderWorld::CreateSnapshot
                        -> Renderer::SubmitRenderWorldSnapshot
            -> Renderer::Tick
                -> RenderPipeline::Execute
                    -> Update scene/light/post data
                    -> Prepare resources
                    -> Build visibility / queues / batches
                    -> Prepare instance and GPU-driven buffers
                    -> Build RenderGraph
                    -> RenderGraph::Execute
            -> Renderer::EndFrame
```

这个顺序是合理的：Renderer 先 acquire frame，Scene 同步新的 RenderWorld snapshot，Renderer 再消费 snapshot 录制 RenderGraph。当前还没有独立 render thread，因此所有这些边界都是单线程帧内边界。

## CMake Dependency Map

当前 CMake 目标层面的公开依赖如下：

| Target | Public dependencies | Notes |
| --- | --- | --- |
| `ChikaEngine` | `ChikaRuntime` | 引擎外壳和 runtime 聚合。 |
| `ChikaRuntime` | all runtime modules | Interface 聚合目标，不提供真实边界。 |
| `ChikaCore` | `ChikaThirdParty` | math、debug、io、reflection、serialization、base containers。 |
| `ChikaProfiler` | private `nlohmann_json` | 横切观测模块。 |
| `ChikaJobs` | `ChikaProfiler` | scheduler 依赖 profiler name/session。 |
| `ChikaAsset` | `ChikaThirdParty`, `ChikaCore`, `ChikaJobs` | asset database/import/load/hot reload/async。 |
| `ChikaRHI` | `ChikaThirdParty`, `ChikaCore` | 后端无关接口和 Vulkan 实现同 target。 |
| `ChikaResource` | `ChikaThirdParty`, `ChikaCore`, `ChikaAsset`, `ChikaRHI` | Asset -> GPU resource adapter。 |
| `ChikaRender` | `ChikaThirdParty`, `ChikaCore`, `ChikaProfiler`, `ChikaJobs`, `ChikaAsset`, `ChikaRHI`, `ChikaResource` | Renderer、RenderWorld、RenderGraph、RenderPipeline。 |
| `ChikaFramework` | `ChikaThirdParty`, `ChikaCore`, `ChikaProfiler`, `ChikaRender`, `ChikaResource`, `ChikaPhysics` | Gameplay/Scene/Component 层，目前耦合最重。 |
| `ChikaInput` | `ChikaCore`, `ChikaThirdParty` | GLFW backend。 |
| `ChikaTime` | `ChikaThirdParty`, `ChikaCore` | GLFW time backend。 |
| `ChikaPhysics` | `ChikaThirdParty`, `ChikaCore` | Jolt backend behind interface。 |
| `ChikaScripts` | `ChikaThirdParty`, `ChikaCore` | Embedded Python/pybind11。 |
| `ChikaProject` | `ChikaAsset`, `ChikaPlatform`, `ChikaRender` | 为 `RenderPipelineMode` 拉入 Render。 |

需要注意一个隐藏点：`ChikaFramework` 源码直接使用 `AssetManager`、`AssetReference`、`AssetHandle`，但 CMake 没有显式链接 `ChikaAsset`，而是通过 `ChikaRender` 或 `ChikaResource` 的 public transitive dependency 获得。这会让真实依赖不够清楚。

## Runtime Module Responsibilities

### Core

`Core` 是基础层，包含：

- math: vector、quaternion、matrix、bounds。
- debug: logging、assert、gizmo line data。
- base: `SlotMap`、handle template、UID generator、fixed step accumulator。
- reflection and serialization support。
- stream abstraction。

`Core` 目前是很多模块的共同依赖，职责基本合理。需要注意的是 reflection/serialization 和 debug 都在 Core 内，后续如果 Core 继续扩大，可能变成“所有东西都能放”的模块。

### Application And EngineContext

`Application` 是主循环模板，`EngineContext` 是显式服务容器。

当前初始化顺序：

```text
Reflection
UIDGenerator
Window
Time
Input
Scripts
Jobs
AssetManager
Renderer
SceneManager
```

当前关闭顺序是逆向的，整体健康。

耦合判断：

- `EngineContext` 高耦合是合理的，因为它就是 composition root。
- 但 `EngineContextCreateInfo` 暴露了 `Render::RHIBackendTypes`、`RenderPipelineMode`、`RenderPathMode`、`RenderCpuMode`，导致只要包含 `EngineContext.hpp` 就会拉入 `Renderer.hpp`。如果之后要让 engine shell 更轻，可以把这些枚举移动到更轻的 runtime settings header。

### Framework

Framework 是 gameplay 层，当前包含：

- `Scene` / `SceneManager`
- `GameObject`
- `Component`
- `Transform`
- `MeshRenderer`
- `CameraComponent`
- `LightComponent`
- `Animator`
- `Rigidbody`
- `RenderSubsystem`
- `PhysicsSubsystem`
- `AnimationSubsystem`
- `Prefab`
- `EventBus`

已经实现得比较完整的部分：

- GameObject/Component 生命周期：`Awake`、`Start`、`FixedTick`、`Tick`、`LateTick`、enable/disable、destroy。
- 延迟销毁和 tick 时安全移除。
- Transform hierarchy、循环防护、跨 scene 拒绝。
- Scene mode: Edit、EnteringPlay、Play、Paused、ExitingPlay。
- Play backup/restore，Stop 后丢弃 runtime-only 对象和运行时属性修改。
- Prefab snapshot capture/instantiate。
- EventBus 驱动 RenderSubsystem 的增量注册。

主要耦合：

- `scene.hpp` 公开 include `PhysicsScene.h`、`Renderer.hpp` 和具体 subsystem header。
- `RenderSubsystem.h` 公开 include `AssetManager.hpp`、`RenderWorld.hpp`、`Renderer.hpp`、`ResourceManager.hpp`。
- `CameraComponent.hpp` 和 `LightComponent.hpp` 直接 include `RenderWorld.hpp`，组件直接返回 `Render::RenderView` / `Render::RenderLightProxy`。
- `MeshRenderer.h` include `AssetManager.hpp`，并在组件里缓存 asset handle。
- `Rigidbody.hpp` include `PhysicsDescs.h`，组件持有 `PhysicsBodyHandle` 并通过 Scene 访问 PhysicsScene。
- `Animator` 的 `finalMatrices` 由 AnimationSubsystem 写入，再由 RenderSubsystem 复制到 Render proxy。

这不是错误，但表示当前 `Framework` 是 gameplay 与 engine service 的桥接层，而不是纯 gameplay 数据层。稳定阶段可以接受这个设计，但应避免让更多 render/RHI 细节进入组件头文件。

### Render

Render 已经有明显分层：

```text
Renderer facade
    -> RenderDeviceContext
        -> IRHIDevice / VulkanRHIDevice
    -> RenderResourceSystem
        -> ResourceManager
    -> RenderPipeline
        -> RenderWorldSnapshot
        -> RenderSceneView
        -> Visibility
        -> RenderQueue / Batch / Instance
        -> RenderGraph
```

稳定成果：

- `Renderer` 不再直接读取 `Scene`、`GameObject`、`Component`。
- `RenderWorldSnapshot` 是 immutable frame input。
- `RenderSceneView` 把 snapshot 分类成 static opaque、skinned、transparent、invalid resource。
- Render preparation 有 serial oracle 和 jobs path，失败回退 serial。
- RenderGraph 支持 Texture、Buffer、Graphics、Compute、Copy、Present、barrier、final state、debug dump。
- RHI binding 由 shader reflection 的 set/binding/type 驱动，不再靠 Vulkan 后端猜 descriptor 类型。

主要耦合：

- `RenderPipeline` 仍然是很大的类，集中拥有 dynamic buffers、light buffer、post process、deferred lighting、gpu-driven culling、pass 构建和 draw 录制。
- `RenderPipeline.hpp` 暴露很多内部私有函数和资源成员。虽然是 private，但头文件变化会影响所有包含它的模块。
- `Render` 仍直接依赖 `Asset`，主要因为 Pipeline 初始化、shader interface 和 ResourceManager 相关类型还在同一层协作。

判断：Render 内部耦合偏高，但边界方向正确。稳定优先级低于 Framework 公开头文件耦合。

### RHI

RHI 目标包含接口和 Vulkan 后端：

- `IRHIDevice`
- `IRHICommandList`
- `RHIDesc`
- `RHIResourceHandle`
- `ResourceBinder`
- `RHIBackendFactory`
- `VulkanRHIDevice`
- `VulkanCommandList`

稳定成果：

- RHI API 已经能表达 graphics/compute pipeline、buffer/texture/sampler/view、resource state、barrier、indirect draw、dispatch。
- Vulkan layout 和 descriptor set layout 按 shader interface 创建。
- Debug names、frame statistics、GPU timings 已进入 RHI。

耦合注意：

- 命名空间仍是 `ChikaEngine::Render`，而模块名是 `RHI`。短期没问题，但文档上要明确 RHI 是 Render namespace 下的硬件抽象子域。
- `VulkanRHIDevice` 公开了一批 raw Vulkan getter，当前只应由 Editor `VulkanAdapter` 使用。Runtime boundary check 已禁止 runtime 引用 ImGui/Editor，但没有禁止非 Editor 使用 Vulkan raw getter。

### Resource

`ResourceManager` 是 Asset 到 GPU resource 的 adapter：

- Mesh asset -> vertex/index buffers。
- Texture asset -> RHI texture。
- Material asset -> shader/pipeline/material UBO/resource bindings。
- Pending upload jobs 交给 RenderGraph copy pass 执行。

稳定成果：

- Resource 不再反向依赖 Render facade。
- Resource 依赖 RHI 和 Asset，方向正确。
- Material UBO layout 使用 shader reflection offset。

耦合注意：

- ResourceManager 不只是“资源表”，它会创建 shader/pipeline，并解析 material draw bindings。这是当前 Renderer 架构下合理的中间层，但不要让 gameplay 直接依赖它。
- Upload staging buffer 的生命周期依赖 Vulkan RHI 延迟删除语义：ResourceManager enqueue upload 后调用 `DestroyBuffer(staging)`，Vulkan 在若干帧后才真实释放。这个隐式契约应保持测试或文档保护。

### Asset

Asset 层当前包含：

- `AssetDatabase`: GUID/meta/index/change polling。
- `ImporterRegistry`: passthrough、shader、scene importer。
- `AssetManager`: cache、load、hot reload、async load。
- loaders: mesh、texture、shader、material、shader template、animation。
- `AssetReference`: GUID identity + optional legacy diagnostic path。

稳定成果：

- Source asset identity 已从 path 迁移到 GUID/meta。
- Project runtime mode 能关闭 packaged game 的 scan/import/hot reload。
- Shader import 会编译 SPIR-V 并生成 reflection sidecar。

耦合注意：

- Development Runtime 仍允许 import/hot reload；Packaged Runtime 通过 boot config 关闭，但 `AssetManager::ResolveReference` 自身并不知道 runtime mode。
- `AssetReference::diagnosticPath` fallback 仍存在。发布路径要确保 packaged registry/cooked content 不依赖 fallback。

### Physics

Physics 结构：

```text
Framework::PhysicsSubsystem
    -> Physics::PhysicsScene
        -> IPhysicsBackend
            -> PhysicsJoltBackend
```

稳定成果：

- 每个 Scene 拥有自己的 PhysicsScene。
- Rigidbody 创建、销毁、velocity、impulse、transform sync 已连通。
- Scene 使用 fixed step accumulator 驱动物理。

耦合注意：

- `Rigidbody` 持有 `PhysicsBodyHandle`，并直接通过 Scene 找 PhysicsScene。
- `PhysicsSubsystem::SyncTransform` 每次 poll 后按 GameObjectID 回写 Transform，代码里已标记为低效。
- Collision events 仍是 TODO。
- `PhysicsInitDesc` 默认重力是 zero，这对游戏预期可能很容易误判。

### Jobs

JobSystem 是独立 scheduler：

- bounded storage/queues。
- dependencies/children。
- wait-help。
- main-thread jobs。
- shutdown drain/cancel policy。
- profiler integration。

Render 和 Asset 都通过注入的 `Jobs::JobSystem*` 使用它，方向健康。当前它依赖 `Profiler` 是合理的横切依赖。

### Project

Project 层负责加载类似 `.uproject` 的 descriptor，并投影为 `RuntimeBootConfig`。

耦合注意：

- `ProjectDescriptor.hpp` include `RenderSettings.hpp`，只是为了 `RenderPipelineMode`。这会让 Project 依赖整个 Render target。
- 如果后续想让 Project 更像纯配置模块，可以把 `RenderPipelineMode` 这类 enum 移到轻量 `RuntimeRenderConfig.hpp`。

### Input, Time, Scripts

这些模块目前都以 singleton/static 形式存在：

- `InputSystem` 持有 static backend。
- `TimeSystem` 持有 static backend 和时间状态。
- `ScriptsSystem` 是 singleton，public header 直接 include `pybind11/embed.h`。

这对单 EngineContext 小项目是可用的。风险是将来多个 context、headless test、hot reload VM 或 server mode 会受全局状态限制。短期建议先记录为单实例契约，不急着重写。

### Editor Boundary

Editor 已经走独立边界：

- `EditorApplication` 继承 `Application`。
- `EditorManager` 持有 panels 和 `EditorContext`。
- ImGui/Vulkan UI 适配放在 `Editor::VulkanAdapter`。
- Runtime boundary check 禁止 Runtime 和 Game 引用 ImGui/Editor。

Editor 显式 dynamic_cast 到 `VulkanRHIDevice` 并使用 raw Vulkan，这是可接受的，因为它在 Editor 层，没有污染 Runtime API。

## Coupling Matrix

| Coupling | Current level | Is it acceptable now | Reason |
| --- | --- | --- | --- |
| `EngineContext -> all runtime services` | High | Yes | Composition root 本来负责组装和生命周期。 |
| `Framework -> Render` | High | Partially | RenderSubsystem 需要 bridge，但组件头文件直接暴露 Render 类型会扩大耦合。 |
| `Framework -> Resource` | Medium/High | Partially | RenderSubsystem 上传资源合理，Gameplay 组件不应继续靠近 ResourceManager。 |
| `Framework -> Asset` | High | Yes, but make explicit | MeshRenderer/Animator/Scene runtime validation 都需要 asset reference；CMake 应显式链接。 |
| `Framework -> Physics` | Medium | Yes | Rigidbody/PhysicsSubsystem 是 gameplay 物理桥，后续可隐藏 handle。 |
| `Render -> Resource/RHI/Asset/Jobs` | Medium | Yes | Render pipeline 的正常下游依赖。 |
| `Resource -> RHI/Asset` | Medium | Yes | Asset-to-GPU adapter 的核心职责。 |
| `Project -> Render` | Low/Medium | Not ideal | 只为 enum/config 拉入 Render，可以轻量化。 |
| `Editor -> Vulkan backend` | Medium | Yes | Editor-only adapter，RuntimeBoundary 已保护 ImGui 不进 runtime。 |
| `Input/Time/Scripts static globals` | Medium | Accept for now | 单 context 小引擎可用，但要明确限制。 |

## Main Stability Risks

1. `Framework` 公开头文件耦合过多。
   - `scene.hpp` 和 component headers 会把 Render/Physics/Asset 细节传递给所有 gameplay include 方。
   - 这会让后续“只改渲染也导致 gameplay 大量重编译”。

2. Bridge 层职责集中。
   - `RenderSubsystem` 同时监听事件、持有组件指针、解析 asset、上传 resource、创建 Render proxy、提交 snapshot。
   - 当前可以接受，但应该把它视为唯一允许读取 Gameplay 的 render bridge，不要再让 RenderPipeline 读取 Scene。

3. Resource upload 依赖后端延迟销毁。
   - 当前 Vulkan 可以工作，但这个语义没有在 `IRHIDevice` 层表达。
   - 如果未来有另一个 RHI 后端，staging buffer 的延迟释放可能出问题。

4. Project config 依赖 Render target。
   - 这会让 boot config 和渲染实现绑得过紧。

5. Global systems 限制多实例。
   - Input/Time/Scripts 使用全局状态。当前单窗口单 runtime 正常，未来多 world/server tests 需要重新处理。

6. Runtime boundary check 还比较粗。
   - 现在只禁止 Editor/ImGui 进入 Runtime/Game。
   - 没有检查 Framework 是否意外 include Vulkan、RHI、Renderer 内部类型，也没有检查 Resource 是否反向 include Render facade。

## What Is Already Stable Enough

可以先稳定，不建议立刻重写的部分：

- `Application` / `EngineContext` 生命周期。
- Scene mode 和 GameObject/Component 生命周期。
- RenderWorld snapshot 边界。
- RenderGraph 基本模型。
- RHI shader reflection binding 路径。
- JobSystem 作为独立 scheduler。
- Editor-only ImGui/Vulkan adapter。

这些部分已经有对应测试或清楚的文档记录，接下来更应该补边界检查和减少头文件耦合，而不是继续大范围重构。

## Recommended Direction

短期目标不是“完全解耦”，而是让耦合只出现在明确边界：

```text
Gameplay data/components
    -> Framework bridge systems
        -> RenderWorld / PhysicsScene / AssetManager
            -> Render / Resource / RHI / Physics backend
```

推荐原则：

- `RenderPipeline` 不能 include 或读取 `Scene/GameObject/Component`。
- `ResourceManager` 不能 include `Renderer` 或 `RenderPipeline`。
- Runtime 不能 include Editor/ImGui。
- Component 头文件尽量只保存 gameplay 数据和 asset reference，具体转换尽量放到 subsystem `.cpp`。
- CMake target dependency 要显式表达源码真实依赖，不靠 transitive dependency 暗中获得。

