# Renderer Step 1.5：Vulkan Reflection Layout Cache

## Metadata

- Date: 2026-06-13
- Status: Complete
- Main files: `VulkanRHIDevice.hpp/.cpp`, `VulkanResource.hpp`

## Goal

由 Shader Interface 创建 Vulkan Layout，并删除 Pipeline 对全局固定 Descriptor Layout 的依赖。

## Implementation

- 按每个 set 的 Reflection Binding 创建并缓存 `VkDescriptorSetLayout`。
- 按资源和 Push Constant 布局 Hash 创建并缓存 `VkPipelineLayout`。
- Pipeline 保存实际 set layouts、资源契约和 Push Constant 范围。
- Layout Cache 由 Device 统一拥有，Pipeline 销毁不提前销毁共享 Layout。
- Vulkan 后端不再存在 `m_globalDescriptorLayout`。

## Verification

- Forward/Deferred Editor 运行无 Layout Validation Error。
- 静态 Pipeline 只声明 location 0-2，蒙皮 Pipeline 声明 location 0-4；Phase 0 的 location 3/4 警告已消失。

## Design Result

不同 Shader 可以拥有不同 Layout，Pipeline Layout 与 Reflection 成为一一对应关系。
