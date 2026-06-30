# RHI Device 与 Command 优化开发日志

## Metadata

- Date: 2026-06-04
- Area: Runtime/Render/RHI
- Related files: `engine/Runtime/Render/include/ChikaEngine/RHIDesc.hpp`, `engine/Runtime/Render/include/ChikaEngine/IRHICommandList.hpp`, `engine/Runtime/Render/include/ChikaEngine/rhi/Vulkan/VulkanCommandList.hpp`, `engine/Runtime/Render/src/rhi/Vulkan/VulkanCommandList.cpp`, `engine/Runtime/Render/include/ChikaEngine/rhi/Vulkan/VulkanRHIDevice.hpp`, `engine/Runtime/Render/src/rhi/Vulkan/VulkanRHIDevice.cpp`, `engine/Runtime/Render/include/ChikaEngine/rhi/Vulkan/VulkanHelper.hpp`
- Status: Complete

## Goal

优化 RHI device 和 command 层，让 pipeline 描述更接近现代渲染管线需求，并为延迟渲染所需的 fullscreen draw 与多 render targets 铺好基础。

## Context

当前 `PipelineDesc` 已经通过 `colorAttachmentFormats` 支持多个颜色附件的描述，Vulkan 后端也会按 vector 创建 blend attachments。但 RHI 抽象仍然缺少几个基础能力：

- command list 只有 `DrawIndexed`，不支持 fullscreen triangle 常用的非索引 `Draw`。
- pipeline 状态只有 `depthTest`、`depthWrite`、`alphaBlendEnable`，拓扑、剔除、正面方向、深度比较等状态隐含在 Vulkan 实现里。
- `VulkanRHIDevice::AllocateCommandList` 使用 `new VulkanCommandList`，`Submit` 只保存 raw command buffer，没有释放 command list 对象，存在 CPU 侧对象泄漏。

本次不大改 `IRHIDevice` 的返回类型，避免牵动 RenderGraph 和 ResourceManager，而是在 Vulkan 后端内部接管已提交 command list 的生命周期。

## Changes

- `RHIDesc.hpp` 增加 `PrimitiveTopology`、`CullMode`、`FrontFace`、`CompareOp`。
- `PipelineDesc` 增加 `topology`、`cullMode`、`frontFace`、`depthCompare`，默认值保持当前渲染行为。
- `IRHICommandList` 增加 `Draw(uint32_t vertexCount, uint32_t instanceCount)`。
- `VulkanCommandList` 实现 `Draw`，内部调用 `vkCmdDraw`。
- `VulkanHelper.hpp` 增加 RHI pipeline 状态到 Vulkan 枚举的转换函数。
- `VulkanRHIDevice::CreateGraphicsPipeline` 改为从 `PipelineDesc` 读取拓扑、剔除、正面方向、深度比较。
- `CreateGraphicsPipeline` 对无 color attachment 的 depth-only pipeline 做了更稳妥的空指针处理。
- `VulkanRHIDevice` 增加每帧 `m_submittedCommandLists`，`Submit` 后接管 command list 对象，下一次对应 frame fence 完成并 reset command pool 前释放。

## Verification

- Commands run: `cmake --build build`
- Result: 构建成功，`ChikaRender`、`ChikaResource` 和 `ChikaEditor` 均完成编译链接，`build/bin/ChikaEditor.exe` 生成成功。
- Remaining warnings or risks: 仍有既有的 `engine/Editor/src/main.cpp` 中 `glfw/glfw3.h` 大小写路径 warning。`AllocateCommandList` 仍然返回 raw pointer，这是为了避免本轮大范围 API 改动。更彻底的设计应该让 `AllocateCommandList` 返回 `std::unique_ptr<IRHICommandList>` 或引入 command allocator/command pool 对象。

## Next Steps

- 将 `IRHIDevice::AllocateCommandList` 改为显式所有权接口，优先考虑 `std::unique_ptr<IRHICommandList>`。
- 为 deferred lighting 增加 fullscreen triangle pipeline，并在 lighting pass 中调用 `Draw(3, 1)`。
- 扩展材质和 shader template，让材质能声明多 color attachment pipeline，而不是默认只创建单 BGRA8 pipeline。
- 在 pipeline 层增加 render state hash 和 cache，避免相同 shader/state 重复创建 pipeline。
