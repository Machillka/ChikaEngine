# Renderer 黑屏修复开发日志

## Metadata

- Date: 2026-06-04
- Area: Runtime/Render/RenderGraph/Editor
- Related files: `engine/Runtime/Render/src/renderer.cpp`
- Status: Complete

## Goal

修复启动后 Editor UI 正常但 Viewport 场景黑屏的问题，并确认 RenderGraph 的 pass 编译结果符合当前 offscreen viewport 渲染链路。

## Context

当前编辑器 viewport 通过 `Renderer::GetOffscreenTexture()` 获取离屏渲染结果，并在 ImGui 的 `ViewportPanel` 中用 `ImGui::Image` 展示。也就是说主场景不是直接写 swapchain，而是先写 `m_rgOffscreen`，再由 ImGui pass 读取 offscreen texture 并写入 swapchain。

前一轮优化 RenderGraph 后，图编译会从有副作用的 pass 和最终输出反向标记 live pass。这个设计本身合理，但要求所有被显示的中间资源都必须接到最终输出链路上。

本次黑屏的直接原因是 `BuildRenderGraph()` 只加入了 upload、shadow、main scene 和 present，没有加入 `AddImGuiPass()`。因此 `Main Scene Pass` 写入的 `m_rgOffscreen` 没有任何消费者，会在 RenderGraph 编译阶段被裁剪；最终场景 pass 不执行，viewport 只能看到未被有效写入的 offscreen 内容。

## Changes

- 在 `Renderer::BuildRenderGraph()` 中将 `AddImGuiPass()` 接回图构建流程。
- 当前图链路恢复为：资源上传 -> 阴影 -> 主场景写 offscreen -> ImGui 读取 offscreen 并写 swapchain -> present。
- 该修复保持 RenderGraph 剔除逻辑不变，通过补全资源依赖关系解决黑屏，而不是关闭 pass culling。

## Verification

- Commands run: `cmake --build build`
- Result: 构建成功，`build/bin/ChikaEditor.exe` 完成链接。
- Commands run: 使用 lldb 在 `renderer.cpp` 的 `AddMainScenePass` 绘制循环断点运行 `build/bin/ChikaEditor.exe`。
- Result: 断点命中，调用栈显示来自 `RenderGraph::Execute()`，`DrawCommand` 中 `meshHandle` 和 `materialHandle` 均有效，说明主场景 pass 已经进入实际执行路径。
- Remaining warnings or risks: 本次验证确认 pass 不再被裁剪；如果后续仍出现局部黑屏，需要继续检查 shader 输出、相机位置、材质纹理上传和 Vulkan validation layer 输出。

## Next Steps

- 为 RenderGraph 增加 debug dump，输出每帧 pass live/cull 状态和资源 producer/consumer 链路。
- 将 Editor viewport 的 offscreen 依赖显式建模为 renderer 输出目标，避免以后重构时再次漏接 ImGui pass。
- 检查 `ResourceManager` 的 staging buffer 延迟上传生命周期，明确上传完成后再释放 staging 资源。
- 为 renderer 增加一个最小 debug clear/triangle pass，用于区分 pass 未执行、资源未上传、shader 未输出三类黑屏原因。
