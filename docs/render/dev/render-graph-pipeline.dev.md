# Render Graph 与渲染管线开发日志

## Metadata

- Date: 2026-06-03
- Area: Runtime/Render
- Related files: `engine/Runtime/Render/src/RenderGraph.cpp`, `engine/Runtime/Render/include/ChikaEngine/RenderGraphPass.hpp`, `engine/Runtime/Render/include/ChikaEngine/Renderer.hpp`, `engine/Runtime/Render/src/renderer.cpp`
- Status: Complete

## Goal

分析并优化当前 RenderGraph 建图过程，避免只依赖简单可达性剔除；同时在 `Renderer` 中建立前向渲染与延迟渲染两条管线路径。

## Context

当前 RenderGraph 已有 pass、texture、producer、生命周期统计和自动 barrier，但 `Compile` 主要从 side-effect pass 反向标记可达节点，然后按插入顺序执行。这样能剔除无用 pass，但没有形成显式执行调度阶段。

当前 RHI/材质系统还有两个限制：材质 pipeline 只按一个 color attachment 创建，RHI 只有 `DrawIndexed`，没有 fullscreen triangle/quad 接口。因此本次延迟渲染先落地 graph 和 renderer pipeline 结构，lighting pass 暂时作为占位 pass 保留真实依赖链。

## Changes

- `RenderGraph::Compile` 改为 producer 分析、live pass 标记、依赖 DFS 调度三个阶段。
- `RGPass` 增加 `isCulled`，让编译后的 pass 状态更明确。
- `RendererCreateInfo` 增加 `RenderPipelineMode pipelineMode`，默认 `Forward`。
- `Renderer` 保存 `m_pipelineMode`，根据模式选择前向或延迟建图路径。
- 前向路径保持现有 `Shadow Pass` + `Main Scene Pass`。
- 延迟路径新增 `Deferred GBuffer Pass` 和 `Deferred Lighting Pass`。
- `Deferred GBuffer Pass` 创建临时 `GBuffer.Albedo`，并写入 depth。
- `Deferred Lighting Pass` 显式读取 `GBuffer.Albedo` 与 `ShadowDepth`，写入 swapchain。

## Verification

- Commands run: `cmake --build build`
- Result: 构建成功，`ChikaRender` 重新编译并链接，`build/bin/ChikaEditor.exe` 生成成功。
- Remaining warnings or risks: 仍有既有的 `engine/Editor/src/main.cpp` 中 `glfw/glfw3.h` 大小写路径 warning。延迟 lighting pass 暂时只建立图依赖和目标输出，还没有真正执行 fullscreen lighting draw。完整延迟渲染还需要 RHI 支持非索引绘制或 fullscreen mesh、专用 lighting shader、GBuffer 材质管线。

## Next Steps

- 给 RHI 增加 `Draw(uint32_t vertexCount, uint32_t instanceCount)`，支持 fullscreen triangle。
- 增加 deferred lighting shader 和对应 pipeline 创建路径。
- 扩展材质/ShaderTemplate，使 pipeline 能声明多 render targets。
- 将 GBuffer 从单 albedo 扩展为 albedo、normal、material 三个 attachment。
- 将 RenderGraph 的资源状态扩展到 imported resource 的初始/最终状态，避免外部资源总是隐式从 `Undefined` 推导。
