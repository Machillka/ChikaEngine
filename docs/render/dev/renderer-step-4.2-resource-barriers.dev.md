# Renderer Step 4.2：资源状态与 Barrier

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RHIDesc.hpp`, `IRHICommandList.hpp`, `VulkanHelper.hpp`, `VulkanCommandList.cpp`

## Goal

用后端无关状态连接 Copy、Compute、Graphics、Depth 和 Present。

## Implementation

- `ResourceState` 增加 Copy、Vertex、Index、Uniform、Storage、Indirect、Depth Read/Write。
- RHI 增加 Buffer Barrier；Texture Barrier 支持 mip/layer `TextureSubresourceRange`。
- Imported Resource 可声明 initial/final state；Graph 在首次使用和最后使用处自动转换。
- Vulkan 映射明确的 Layout、Access Mask 和 Pipeline Stage。

## Design Reason

状态属于使用方式而不是资源类型。让 Pass 声明状态、由 Graph 生成 Barrier，可避免高级渲染功能直接调用 Vulkan 同步 API。

## Verification

专项测试验证 Buffer Barrier 数量；Forward 与 Deferred 编辑器运行均正常退出。
