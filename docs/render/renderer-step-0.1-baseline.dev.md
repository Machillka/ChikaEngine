# Renderer Step 0.1：渲染契约与基准场景

## Metadata

- Date: 2026-06-13
- Area: Runtime/Render/RHI/Tests/Editor
- Related files:
  - `engine/Editor/src/main.cpp`
  - `engine/Runtime/Render/include/ChikaEngine/RenderGraph.hpp`
  - `engine/Runtime/Render/src/RenderGraph.cpp`
  - `tests/unit/RenderBaselineTests.cpp`
  - `tests/CMakeLists.txt`
- Status: Complete

## Goal

冻结 Renderer 架构升级前的基础行为，建立不依赖真实 GPU 的 RenderGraph 测试和可重复运行的 Editor 基准场景。

## Current Contract

当前必须保持的渲染数据流：

```text
Scene
    -> RenderSubsystem::CollectDrawCommands
    -> Renderer::SubmitDrawCommands
    -> Renderer::BuildRenderGraph
    -> RenderGraph::Compile / Execute
    -> RHI Command List
```

Phase 0 只建立测试和诊断，不改变以上边界。`RenderWorld`、剔除、排序和批处理属于后续 Phase。

### RenderGraph 行为契约

- `Compile()` 从有 Side Effect 的 Pass 反向标记存活依赖。
- 无法到达 Present/Upload 等 Side Effect Pass 的节点会被剔除。
- Producer 必须在 Consumer 前执行。
- `GetCompiledPassNames()` 只返回实际存活 Pass 的执行顺序。
- `GetLastExecutedPassCount()` 记录最近一次实际执行数量。
- 每个实际执行 Pass 必须生成同名 Debug Label。

### DrawCommand 行为契约

- 默认 Mesh、Material 和 Bone Buffer Handle 无效。
- 默认对象不是 Skinned Mesh。
- 当前 Gameplay 仍负责每帧生成 DrawCommand。

## Fixed Baseline Scene

Editor 启动时由 `CreateRenderBaselineScene()` 创建固定场景：

| Object | Coverage |
|---|---|
| `Baseline.Skinned.Fox` | Skinned Mesh、Animator、Bone Buffer、Rigidbody |
| `Baseline.Static.Floor` | Static Mesh、Static Material、World Transform |
| `Baseline.Pending.MultiMaterial` | 无渲染组件的单对象多材质迁移占位 |
| `Baseline.Pending.Transparent` | 无渲染组件的透明材质迁移占位 |

当前负向能力基线：

| Feature | Baseline Status | Reason |
|---|---|---|
| 单对象多材质 | Unsupported | `MeshRenderer` 只能保存一个 Material |
| 透明材质 | Unsupported placeholder | Material/Pipeline 没有正式 Blend Mode |
| 多光源 | Unsupported | Renderer 仍使用单全局光 |
| Frustum Culling | Unsupported | Mesh 尚无 Bounds |
| Instancing | Unsupported | 每个对象仍产生独立 Draw |

这些能力不能用错误结果伪装成已支持，后续实现时必须更新本表和基线统计。
两个 `Baseline.Pending.*` 对象只作为 Scene Hierarchy 中的迁移锚点，不产生 Draw Call。

## Baseline Statistics

统计对象为固定场景，不包含 ImGui 后端内部 Draw Call。

### Forward

| Frame | Passes | Draw Calls | Instances | Pipeline Binds | Descriptor Writes |
|---|---:|---:|---:|---:|---:|
| First frame | 5 | 4 | 4 | 4 | 20 |
| Steady frame | 4 | 4 | 4 | 4 | 20 |

首帧比稳定帧多一个 `Upload Resources` Pass。

### Deferred

| Frame | Passes | Draw Calls | Instances | Pipeline Binds | Descriptor Writes |
|---|---:|---:|---:|---:|---:|
| First frame | 6 | 5 | 5 | 5 | 24 |
| Steady frame | 5 | 5 | 5 | 5 | 24 |

Deferred 比 Forward 多一个 Fullscreen Lighting Draw。

## Changes

- 新增 `Chika.RenderBaseline` 单元测试。
- 使用 Mock RHI 验证 RenderGraph Pass 剔除、依赖顺序、执行数量和 Debug Label 顺序。
- 冻结 `DrawCommand` 默认值契约。
- 将 Editor 初始场景整理为具名的固定渲染基准场景。
- 增加 RenderGraph 编译顺序和实际 Pass 数量查询接口。

## Verification

- Commands run: `cmake -S . -B build`
- Result: Configure successful.
- Commands run: `cmake --build build`
- Result: Build successful.
- Commands run: `ctest --test-dir build --output-on-failure`
- Result: 4/4 tests passed, including `Chika.RenderBaseline`.
- Commands run: LLDB at `Renderer::EndFrame`.
- Result: Forward and Deferred first/steady frame statistics match the tables above.

## Remaining Risks

- 当前基准只验证提交结构，不进行截图像素对比。
- 多材质和透明仍是明确的未支持能力。
- `test.json` 引用了不存在的 `test.template.json`，不能作为基准资产使用。
- Pipeline 仍固定声明完整顶点布局，Validation Layer 会报告静态 Shader 未消费 location 3/4。

## Next Steps

- Phase 1 使用 Shader Reflection 生成真实 Vertex Input 和 Pipeline Layout。
- 实现多材质和透明后，扩展固定基准场景与统计表。
- 在后续图像质量阶段加入截图回归。
