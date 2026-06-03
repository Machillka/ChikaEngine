# Renderer 管线选择 UI 开发日志

## Metadata

- Date: 2026-06-04
- Area: Editor/Render
- Related files: `engine/Editor/src/ViewportPanel.cpp`, `engine/Editor/src/main.cpp`
- Status: Complete

## Goal

将渲染管线选择从命令行参数迁移到编辑器 UI，使用户可以在运行时从 Viewport 面板切换 Forward 和 Deferred。

## Context

上一轮实现了 `RenderPipelineMode::Forward` 和 `RenderPipelineMode::Deferred`，并临时通过 `--deferred` 参数验证 deferred 路径。这个入口适合调试，但不适合编辑器工作流。真正的编辑器应当提供可见、可交互的渲染设置入口，并让 Renderer 在下一帧按当前模式重建 RenderGraph。

## Changes

- 移除 `main` 中的 `--deferred` / `--forward` 命令行解析。
- 在 `ViewportPanel` 顶部加入 `Forward` / `Deferred` 下拉选择器。
- 选择变化时调用 `Renderer::SetPipelineMode`，下一帧 `BuildRenderGraph` 会使用对应路径。
- 保持默认启动为 Forward，避免改变现有编辑器默认表现。

## Verification

- Commands run: `cmake --build build`
- Result: 构建成功，`build/bin/ChikaEditor.exe` 完成链接。
- Remaining warnings or risks: UI 选择当前只保存在运行时，没有持久化到项目配置。后续应迁移到 renderer settings/project settings。

## Next Steps

- 增加 Render Settings 面板，集中管理 pipeline mode、debug view、shadow quality、MSAA、HDR 等。
- 将 pipeline mode 持久化到项目配置文件。
- 为 deferred 增加 GBuffer debug visualization，方便确认切换结果。
