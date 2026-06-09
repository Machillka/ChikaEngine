# Renderer 前向与延迟渲染管线开发日志

## Metadata

- Date: 2026-06-04
- Area: Runtime/Render/Resource/RHI/Assets
- Related files: `engine/Runtime/Render/include/ChikaEngine/Renderer.hpp`, `engine/Runtime/Render/src/renderer.cpp`, `engine/Runtime/Resource/include/ChikaEngine/ResourceLayout.hpp`, `engine/Runtime/Resource/src/ResourceManager.cpp`, `engine/Runtime/Render/src/rhi/Vulkan/VulkanRHIDevice.cpp`, `engine/Editor/src/main.cpp`, `Assets/Shaders/gbuffer.frag`, `Assets/Shaders/fullscreen.vert`, `Assets/Shaders/deferred_lighting.frag`
- Status: Complete

## Goal

实现可运行的前向渲染和延迟渲染双管线，并在 RHI 后端补齐多 render targets 与 fullscreen triangle 所需的缺失能力。

## Context

此前 renderer 中已有 `RenderPipelineMode`、`AddGBufferPass` 和 `AddDeferredLightingPass` 的雏形，但实际仍只有 forward scene pass 能输出画面。Deferred 路径存在几个缺口：

- GBuffer 只有单个 albedo attachment。
- Material 只有一个 forward pipeline，无法在 GBuffer pass 中绑定多 render target pipeline。
- Deferred lighting pass 没有 fullscreen shader/pipeline，也没有 draw call。
- Vulkan pipeline 创建固定声明 vertex input，不适合 fullscreen triangle。

本次实现保持 forward 作为默认路径，同时允许初始化或运行时切换到 deferred。

## Changes

- `MaterialGPU` 增加 `forwardPipeline` 和 `gbufferPipeline`，保留 `pipeline` 兼容旧路径。
- `ResourceManager::_UploadMaterial` 为每个材质创建 forward pipeline 和 GBuffer pipeline。
- GBuffer pipeline 使用三个 color attachments：albedo、normal、material。
- `Renderer::BuildRenderGraph` 根据 `RenderPipelineMode` 选择 forward 或 deferred 图。
- Forward 图链路：upload -> shadow -> main scene -> ImGui -> present。
- Deferred 图链路：upload -> shadow -> GBuffer -> deferred lighting -> ImGui -> present。
- `AddGBufferPass` 输出 albedo、normal、material 三个 transient render graph texture。
- `AddDeferredLightingPass` 读取 GBuffer 并用 fullscreen triangle 写入 offscreen color。
- 增加 `Renderer::DrawSceneGeometry`，统一 shadow、forward、gbuffer 的 draw command 录制。
- 增加 `Renderer::CreateDeferredResources`，创建 fullscreen lighting shader 和 pipeline。
- Vulkan pipeline 创建支持空 vertex input，用于 fullscreen triangle。
- Editor 增加命令行开关：默认 forward，`--deferred` 使用 deferred，`--forward` 强制 forward。
- 新增并编译 shader：`gbuffer.frag(.spv)`、`fullscreen.vert(.spv)`、`deferred_lighting.frag(.spv)`。

## Verification

- Commands run: `glslc` 编译新增 shader，并重编 `test.vert`、`test.frag`、`skinned.vert`。
- Result: 所有 shader 成功生成 SPIR-V。
- Commands run: `cmake --build build`
- Result: 构建成功，`build/bin/ChikaEditor.exe` 完成链接。
- Commands run: 使用 lldb 默认参数运行到 `Renderer::EndFrame`。
- Result: Forward 默认路径完成首帧。
- Commands run: 使用 lldb 携带 `--deferred` 运行，断点命中 `Renderer::AddGBufferPass`。
- Result: Deferred 图进入 GBuffer pass。
- Commands run: 使用 lldb 携带 `--deferred` 运行到 `Renderer::EndFrame`。
- Result: Deferred 路径完成首帧。
- Remaining warnings or risks: 构建仍有既有 `InspectorPanel.cpp` 的 `strncpy` deprecated warning。Deferred lighting 当前是基础方向光模型，尚未做完整 PBR、shadow resolve、depth reconstruction 或 debug visualization。

## Next Steps

- 为 deferred lighting 增加 depth/position reconstruction，并接入 shadow map。
- 将 GBuffer format 和 attachment 布局从硬编码迁移到 renderer pipeline 配置。
- 为 pipeline 创建增加 cache，避免每个材质重复创建等价 GBuffer pipeline。
- 将 shader 编译纳入 CMake 或资产构建脚本，避免手动维护 `.spv`。
- 增加 RenderGraph debug dump，输出 pass 顺序、resource producer/consumer 和 attachment format。
