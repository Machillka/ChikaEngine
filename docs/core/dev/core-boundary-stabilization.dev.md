# 核心边界稳定化开发日志

## Metadata

- Date: 2026-06-09
- Area: Runtime/Application/Framework/Render/RHI/Resource/Tests
- Related files: `engine/include/ChikaEngine/Application.hpp`, `engine/include/ChikaEngine/EngineContext.hpp`, `engine/src/Application.cpp`, `engine/src/EngineContext.cpp`, `engine/Runtime/RHI/`, `engine/Runtime/Render/`, `engine/Runtime/Resource/CMakeLists.txt`, `engine/Runtime/Framework/`, `engine/Runtime/Core/src/math/vector3.cpp`, `engine/Runtime/Scripts/`, `tests/`
- Status: Complete

## Goal

稳定引擎核心模块边界：统一管理应用与引擎模块生命周期，解除 Render/Resource 循环依赖，修正 Gameplay 时间与物理基础行为，并建立可重复运行的核心测试和最小集成验证。

## Context

此前编辑器入口直接创建并串联 Window、Renderer、Scene、Input、Time 和 Scripts，初始化失败与关闭顺序缺少统一控制。Render 同时承载 RHI，导致 Resource 为访问 GPU 抽象而反向依赖 Render；Scene 使用可变帧时间直接驱动物理；动画时间被重复推进；Rigidbody 的默认状态和启停生命周期不完整。

本次参考 Unreal Engine 的职责分层，但保持当前项目规模所需的最小实现：

- `Application` 类似应用外壳，拥有主循环和应用级扩展钩子。
- `EngineContext` 类似精简后的 Engine/GameInstance 服务容器，统一拥有核心模块和 `SceneManager`，并按逆序关闭。
- `Scene` 保持接近 World 的职责，管理子系统、Play/Edit 状态和固定物理步长。
- 独立 `RHI` 作为 Render 与 Resource 共同依赖的底层硬件抽象，禁止 Resource 反向依赖 Render。

## Changes

- 新增 `Application` 与 `EngineContext`，统一管理 Window、Time、Input、Scripts、Asset、Renderer 和 Scene 的初始化、主循环与逆序关闭。
- `EngineContext` 通过 `SceneManager` 管理活动 Scene；构造函数在实现文件中定义，使 `unique_ptr<SceneManager>` 保持前置声明边界且可正确析构。
- 编辑器入口改为 `EditorApplication`，编辑器初始化和关闭通过 `OnInitialize`、`OnUpdate`、`OnShutdown` 接入应用生命周期。
- 应用异常路径会清理部分初始化的应用对象；Renderer、Scene、Time、Input、Scripts 和编辑器适配层支持幂等关闭。
- 明确 ImGui 生命周期：Editor Adapter 管理 ImGui Context 及 GLFW/Vulkan 后端，RHI 仅管理 Vulkan descriptor pool 和设备资源，避免重复关闭。
- 将 RHI 接口、描述、资源句柄、后端工厂和 Vulkan 实现从 Render 移入独立 `ChikaRHI` 目标。
- CMake 依赖方向调整为 `ChikaRender -> ChikaResource -> ChikaRHI`，同时 `ChikaRender -> ChikaRHI`；`ChikaResource` 不再链接 `ChikaRender`。
- 新增 `FixedStepAccumulator`，Scene 以默认 60 Hz 固定步长驱动物理，并限制单帧最大物理步数以避免追帧失控。
- 动画时间只由 `Animator::UpdateTime` 推进一次，统一处理播放速度、循环、负速度和非循环钳制。
- Rigidbody 补齐确定性默认值、启用/禁用/脏状态重建、析构销毁队列和未初始化 PhysicsScene 的保护。
- Component 的启用状态切换结合所属 GameObject 的层级激活状态，在真实状态变化时调用 `OnEnable`/`OnDisable`。
- 补齐 `Vector3 * Vector3` 逐分量乘法实现，支持层级 Transform 的世界缩放计算。
- 脚本系统移除运行时 `uv sync`，改为由 CMake 注入 CPython Runtime Home，并通过 `PyConfig` 启动嵌入式 Python。
- 新增核心边界单元测试和 Scene 最小集成测试，并接入 CTest。
- 删除未参与构建的旧空文件 `engine/engine.h` 与 `engine/engine.cpp`。

## Verification

- Commands run:
  - `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build`
  - `ctest --test-dir build --output-on-failure`
  - 启动 `build/bin/ChikaEditor.exe`，运行约 5 秒后通过窗口关闭流程退出
  - 使用 `rg` 审计 Render、Resource、RHI 的链接与包含方向
  - `git diff --check`
- Result:
  - CMake 配置成功，并确认 CPython Runtime Home。
  - `ChikaEditor`、`ChikaCoreBoundaryTests` 和 `ChikaSceneIntegrationTests` 构建成功。
  - CTest 通过 2/2：固定步长、动画时间、Rigidbody 默认值、层级 Component 生命周期、Vector3 逐分量乘法、反射、UID 和 Scene Play/Edit 快照恢复均通过。
  - 编辑器完整初始化并正常关闭，退出码为 `0`。
  - `ChikaResource` 只依赖 `ChikaRHI`，未发现 Resource/RHI 对 Renderer、RenderGraph、ResourceManager 或 AssetManager 的反向包含。
- Remaining warnings or risks:
  - 反射解析器仍打印既有 Clang 诊断，但当前生成与构建成功。
  - `InspectorPanel.cpp` 仍有两处既有 `strncpy` 弃用 warning。
  - 当前 `EngineContext` 是明确所有权容器，尚未实现 Unreal 风格的模块依赖图、模块热加载和多 World/GameInstance。
  - Framework 当前使用未带 `CONFIGURE_DEPENDS` 的源文件 glob；新增 `.cpp` 后需要重新运行 CMake 配置。

## Next Steps

- 引入轻量 `IEngineModule` 与显式依赖/启动阶段描述，让模块顺序从 `EngineContext` 手写流程演进为可验证的模块图。
- 将运行时 Game Application 与 Editor Application 分离，支持无编辑器启动、服务端/测试启动和可配置模块集合。
- 继续完善 SceneManager/World 边界，增加关卡切换事务、异步加载和明确的 BeginPlay、EndPlay、FixedUpdate、销毁队列阶段。
- 为 RHI、Renderer 和 Asset/Resource 增加独立的生命周期集成测试与无窗口测试后端。
- 建立 CI，固定执行配置、构建、CTest 和模块依赖方向检查。
