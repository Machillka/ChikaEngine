# Renderer Step 3.6：GPU Instancing

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderPipeline.cpp`, `IRHICommandList.hpp`, `VulkanCommandList.cpp`, `VulkanRHIDevice.cpp`, `test.vert`, `skinned.vert`

## Goal

让共享 Mesh 与 Material 的静态对象通过一次 Instanced Draw 提交。

## Implementation

- RHI `Draw/DrawIndexed` 支持完整的 instance count、first vertex/index、vertex offset 和 first instance。
- RenderPipeline 使用三缓冲 CPU-to-GPU Storage Buffer 保存本帧实例矩阵；Buffer 仅在容量增长时重建。
- Batch 在写入 Instance Buffer 时记录 `firstInstance`，Shader 使用 `gl_InstanceIndex` 读取 World Matrix。
- `instances` 位于约定的 Object Set，由 Shader Reflection 解析并驱动 Descriptor Layout，不新增硬编码 binding。

## Design Reason

实例数据由 RenderPipeline 在帧级上传，而不是复制到每个 Gameplay Component。这与 Unreal 将实例化数据作为渲染侧场景数据处理的边界一致，也为后续 GPU Scene 提供迁移入口。

## Verification

- 基准场景包含 Floor 与四个相同 Box；它们在主 Pass 和 Shadow Pass 中分别形成一次 Instanced Draw。
- Play Mode Forward 统计为 `4 draw calls / 12 instances / 2 instanced batches`。
- Vulkan Validation 未报告 Descriptor、Buffer 或 Draw 参数错误。

## Remaining Boundary

三缓冲为当前帧资源模型下的过渡实现；后续应由统一 Frame Upload Allocator 管理，并扩展实例颜色、自定义数据和间接绘制。
