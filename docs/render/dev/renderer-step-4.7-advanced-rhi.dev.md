# Renderer Step 4.7：高级 RHI 最小能力集

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `IRHIDevice.hpp`, `IRHICommandList.hpp`, `RHIDesc.hpp`, `ResourceBinder.hpp`, `VulkanRHIDevice.cpp`

## Goal

补齐 Phase 5 高级渲染功能需要的后端无关 RHI 入口。

## Implementation

- Compute Pipeline、Dispatch、Draw Indirect、Draw Indexed Indirect。
- Buffer Usage 改为组合标志，支持 Storage、Transfer、Indirect。
- 独立 Sampler 可创建、绑定、命名和延迟销毁。
- 显式 Texture View 可绑定 mip/layer 子资源；Descriptor Array 使用 Reflection `arrayCount` 和 `arrayElement` 校验。
- Storage Buffer 已贯通 Reflection Descriptor 与 Vulkan Binding。
- Frame 动态上传使用三帧 Buffer 策略；所有 Buffer、Texture、Shader、Pipeline、Sampler、View 均通过 Fence 安全窗口延迟销毁。

## Design Reason

高级功能只能依赖 RHI 契约，Render Pipeline 不包含 Vulkan 类型或直接 Vulkan 调用。

## Verification

RHI Mock 测试覆盖 Sampler、Texture View、Descriptor Array 与间接绘制统计；全量构建和测试通过。

## Remaining Boundary

Phase 4 保留最小 Frame Upload 策略；统一 Upload Arena、异步 Compute Queue 和 Buffer View 属于后续性能扩展，不阻塞 Phase 5 功能实现。
