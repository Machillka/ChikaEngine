# Renderer Step 0.2：渲染诊断与命名

## Metadata

- Date: 2026-06-13
- Area: Runtime/Render/RHI/Resource/Editor
- Related files:
  - `engine/Runtime/RHI/include/ChikaEngine/RenderDiagnostics.hpp`
  - `engine/Runtime/RHI/include/ChikaEngine/IRHIDevice.hpp`
  - `engine/Runtime/RHI/include/ChikaEngine/IRHICommandList.hpp`
  - `engine/Runtime/RHI/src/Vulkan/VulkanRHIDevice.cpp`
  - `engine/Runtime/RHI/src/Vulkan/VulkanCommandList.cpp`
  - `engine/Runtime/Render/src/RenderGraph.cpp`
  - `engine/Runtime/Render/src/renderer.cpp`
  - `engine/Runtime/Resource/src/ResourceManager.cpp`
  - `engine/Editor/src/RenderStatisticsPanel.cpp`
- Status: Complete

## Goal

为 Renderer 升级建立稳定的可观测接口，使 Validation Layer、RenderDoc、Editor 和自动测试能够使用相同的 Pass、资源名称和帧统计。

## Diagnostics Contract

### Frame Statistics

`RenderFrameStatistics` 保存：

| Field | Meaning |
|---|---|
| `passCount` | RenderGraph 实际执行的 Pass 数量 |
| `drawCallCount` | ChikaEngine 显式录制的 Draw/DrawIndexed 数量 |
| `instanceCount` | 所有显式 Draw 的 instanceCount 总和 |
| `pipelineBindCount` | RHI `BindPipeline` 调用数量 |
| `descriptorUpdateCount` | `vkUpdateDescriptorSets` 实际写入的 Descriptor 数量 |

统计不包含 ImGui Vulkan 后端内部生成的 Draw Call。这样统计不会随 ImGui 实现变化而污染 Renderer 性能基线。

RHI 在 `BeginFrame()` 清空命令统计；RenderGraph 记录 Pass 数量；Renderer 在 RenderGraph 执行后汇总两者。

### Debug Names

RHI 新增以下后端无关接口：

```cpp
SetDebugName(BufferHandle, name)
SetDebugName(TextureHandle, name)
SetDebugName(ShaderHandle, name)
SetDebugName(PipelineHandle, name)
IRHICommandList::SetDebugName(name)
IRHICommandList::BeginDebugLabel(name, color)
IRHICommandList::EndDebugLabel()
```

Vulkan 后端使用 `VK_EXT_debug_utils`：

- Buffer、Image、ImageView、Shader Module、Pipeline、Pipeline Layout 和 Command Buffer 可具名。
- 每个 RenderGraph Pass 生成同名命令标签。
- Validation Warning/Error 转发到 ChikaEngine Log System。
- Loader 的 Verbose/Info 消息被过滤，避免有效错误被噪声淹没。

### Naming Rules

- Renderer 内部资源使用 `Renderer.*`。
- Material GPU 资源使用 `Material.<name>.*`。
- Mesh 和 Texture 资源名称包含资产路径。
- RenderGraph 帧命令列表使用 `RenderGraph.Frame`。
- Pass Label 直接使用 RenderGraph Pass 名称。

名称只用于诊断，不参与资源身份、缓存 Key 或生命周期。

## Editor Integration

新增 `Render Statistics` 面板，显示最近完成帧的：

- Passes
- Draw Calls
- Instances
- Pipeline Binds
- Descriptor Writes

Editor UI 在下一帧开始前读取上一帧统计，避免读取正在录制的可变数据。

## Implementation Notes

- 新增函数均补充了用途与设计原因注释。
- Debug Utils 函数指针按需加载；诊断接口不可用时静默跳过，不改变渲染行为。
- Pipeline 命名时同时命名 Pipeline Layout。
- Texture 命名时同时命名 Image 与 ImageView。
- Descriptor 统计按实际 Write 数量计算，而不是只统计 Descriptor Set 数量。

## Verification

- Commands run: `cmake --build build`
- Result: Build successful.
- Commands run: `ctest --test-dir build --output-on-failure`
- Result: 4/4 tests passed.
- Commands run: Forward Editor hidden run for 5 seconds.
- Result: Exit code 0.
- Commands run: Deferred pipeline LLDB run.
- Result: Completed first and steady frames; statistics collected.
- Validation result: No new Validation Error.

## Resolved Validation Warnings

Phase 1 Shader Reflection 已解决以下既有警告：

```text
Vertex attribute at location 3 not consumed by vertex shader.
Vertex attribute at location 4 not consumed by vertex shader.
```

原因为 `ResourceManager::BuildDefaultVertexLayout()` 固定声明 Bone Index/Weight，而静态 Vertex Shader 不使用这些输入。当前 Pipeline Vertex Layout 已按 Reflection 输入生成。

## Next Steps

- Phase 1.1 定义后端无关 Shader Interface。
- Phase 1.2 使用 SPIRV-Reflect 在导入阶段提取 Shader 接口。
- 后续优化必须使用本阶段统计对比 Draw、Instance、Pipeline Bind 和 Descriptor Write 变化。
