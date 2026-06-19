# Runtime Step 0.3：外移 Editor UI 渲染边界

## Metadata

- Status: Complete
- Depends on: Step 0.2
- Suggested scope: `Runtime/Render`, `Runtime/RHI`, `Editor`, tests

## Goal

移除通用 Render/RHI 公共接口中的 ImGui 专用 API，让 Standalone Runtime 不需要携带 Editor UI 渲染职责。

## Planned Changes

- 从通用 RHI/Render 边界移除或隔离 `InitializeImgui`、`DrawImGui`、`GetImGuiTextureHandle` 和 `SubmitImGuiData`。
- 将 ImGui Context、后端初始化、纹理适配和 UI Pass 所有权放入 Editor。
- 若 Editor 需要访问 Vulkan Command Buffer，使用明确的 Editor-only Backend Adapter，不污染通用 RHI 命令接口。
- Render Pipeline 只保留通用可选 Overlay/Extension Pass 边界，不理解 ImGui 类型。
- 增加静态依赖检查，保证 `ChikaGame` 和通用 Runtime 目标不链接 ImGui。

## Minimum Deliverable

- `ChikaGame` 链接闭包中不包含 Editor 和 ImGui。
- `ChikaEditor` 的 Dockspace、Viewport 和 Panel 渲染保持可用。
- 通用 `IRHIDevice`、`IRHICommandList` 和 `Renderer` 公共 API 不出现 ImGui 名称或类型。

## Done When

- Runtime/Render/RHI 可在不构建 Editor/ImGui 的配置下编译。
- Editor UI 仍能显示场景纹理并提交 UI 绘制。
- 自动依赖检查覆盖 Runtime 和 Editor 两个目标。

## Verification

- 构建仅 Runtime/Game 的配置。
- 构建并启动 `ChikaEditor`。
- 静态搜索 Runtime 公共接口中不存在 ImGui API。
- 启动 `ChikaGame`，确认无 Editor UI 初始化。

## Implementation

- 从 `IRHIDevice`、`IRHICommandList`、`Renderer`、`RenderPipeline` 和 Vulkan Runtime 资源中移除所有 ImGui 专用 API、状态与包含。
- `Renderer::SetOverlayPassCallback()` 提供后端无关的可选最终 Overlay 录制点；Render Pipeline 只负责调度 `Overlay Composite Pass`。
- `Editor::VulkanAdapter` 现在独立拥有 UI Context、GLFW/Vulkan Backend、Descriptor Pool、Viewport Texture Descriptor 和 Vulkan Command Buffer 适配。
- `EditorContext::resolveTextureForUi` 将 Panel 与 Vulkan Adapter 解耦；Viewport 不再通过通用 RHI 获取 UI Texture Handle。
- `imgui` 不再由 `ChikaThirdParty` 向 Runtime 传递，只由 `ChikaEditor` 显式链接。

## Verification Result

- Runtime 静态扫描中不存在 ImGui/Backend API。
- Game-only Release 配置中不存在 `engine/ThirdParty/imgui` 构建目录，`build.ninja` 与 `compile_commands.json` 不包含 `imgui` 或 `ChikaEditor`。
- `ChikaEditor` 运行约 5 秒并正常退出，退出码 `0`。
- `ChikaGame` Smoke 启动日志不包含 Editor UI 初始化。

## Design Reason

通用 Renderer 只定义“何时可以录制最终 Overlay”，具体 UI SDK、Descriptor 所有权和 Vulkan Command Buffer 访问由明确依赖 Vulkan 的 Editor Adapter 处理。这样未来 Runtime UI 或其他工具 Overlay 可以复用调度点，而不会让 RHI 理解 ImGui。

## Not Included

- 不实现 Runtime 游戏 UI 系统。
- 不引入新的通用 UI 第三方依赖。

## Next Step

- Step 0.4：建立可独立选择的 Game/Editor/Tool 构建配置。
