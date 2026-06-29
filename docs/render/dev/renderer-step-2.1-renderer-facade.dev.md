# Renderer Step 2.1：Renderer Facade

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `Renderer.hpp/.cpp`, `RenderDeviceContext`, `RenderResourceSystem`, `RenderPipeline`, `RenderSettings`

## Goal

缩小 `Renderer` 的职责，使高层入口不再直接保存 Pass 资源、RenderGraph Handle 和业务常量。

## Unreal Reference

参考 Unreal 将高层 Renderer、Scene、资源与 RHI 生命周期分离的原则，但当前保持单线程，不引入 Render Thread、ENQUEUE 宏或复杂命令队列。

## Implementation

- `Renderer`：生命周期与编辑器 API Facade。
- `RenderDeviceContext`：RHI 创建、帧生命周期和 Swapchain Resize。
- `RenderResourceSystem`：管理 `ResourceManager` 生命周期。
- `RenderPipeline`：消费 Snapshot，组织 Forward、Deferred、Shadow、Upload、ImGui Pass。
- `RenderSettings`：保存 Pipeline Mode 和阴影分辨率。
- `RenderPipeline` 显式释放自己创建的 Texture、Buffer、Shader 和 Pipeline；RHI 设备销毁只作为最终兜底。

## Verification

- Renderer 公共头文件不再保存 Pass Texture、Buffer、Pipeline 或 RenderGraph Handle。
- Forward/Deferred Pipeline 行为保持不变。

## Remaining Boundary

Pass 仍集中在 `RenderPipeline`；独立 Pass Module 与 Blackboard 属于 Phase 4.5。
