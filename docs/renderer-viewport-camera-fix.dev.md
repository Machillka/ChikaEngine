# Renderer Viewport 相机与显示方向修复开发日志

## Metadata

- Date: 2026-06-04
- Area: Runtime/Render/Editor
- Related files: `engine/Runtime/Render/src/renderer.cpp`, `engine/Editor/src/ViewportPanel.cpp`
- Status: Complete

## Goal

修复编辑器 viewport 中模型上下颠倒，以及右键漫游时相机移动失效的问题。

## Context

当前 viewport 通过 `Renderer::GetOffscreenTexture()` 拿到离屏渲染结果，再由 `ViewportPanel` 使用 `ImGui::Image` 显示。Vulkan offscreen texture 进入 ImGui 显示时需要明确处理 V 方向，否则图像可能在编辑器窗口中上下颠倒。

相机移动由 `ViewportPanel::Tick` 在右键漫游状态下驱动，但它依赖 `renderer->GetActiveCamera()`。当前 `Renderer` 类中有 `m_defaultCamera` 成员，但初始化阶段没有创建默认相机，也没有将其设置为 active camera，导致漫游逻辑直接返回。

## Changes

- `Renderer::Initialize` 创建默认 `Camera`。
- 默认相机位置设置为 `(0, 4, 8)`，并朝向世界原点。
- 默认相机使用当前 viewport aspect 初始化透视投影。
- `m_activeCamera` 指向默认相机，保证没有外部相机注入时 editor viewport 仍可移动。
- `ViewportPanel` 显示 offscreen texture 时通过 `ImGui::Image(..., ImVec2(0, 1), ImVec2(1, 0))` 翻转 V 轴。
- 射线检测逻辑保持不变，因为当前反馈中射线检测正常，避免引入额外坐标反转。

## Verification

- Commands run: `cmake --build build`
- Result: 构建成功，`build/bin/ChikaEditor.exe` 完成链接。
- Commands run: 使用 lldb 在 `Renderer::BeginFrame` 处断点检查 `m_activeCamera` 和 `m_defaultCamera`。
- Result: `m_activeCamera` 为有效地址，并与 `m_defaultCamera` 指向同一个 Camera 对象，确认漫游逻辑不再因为空 active camera 被阻断。
- Remaining warnings or risks: 实际 WASD/鼠标漫游需要交互运行确认。如果移动仍不响应，下一步应检查 ImGui hover/focus 状态、右键按下状态和 Docking 多视口输入转发。

## Next Steps

- 将默认 editor camera 从 Renderer 内部临时对象迁移到 Editor/Scene 层，由编辑器明确管理 active camera。
- 为 viewport 增加小型状态显示或 debug log，输出 hovered/focused/roaming/camera position，便于定位输入问题。
- 统一 viewport 显示 UV、ray unproject、gizmo 绘制三者的坐标约定。
- 给 camera controller 增加独立类，避免输入逻辑长期散落在 `ViewportPanel`。
