# Vulkan ImGui Swapchain Validation 修复开发日志

## Metadata

- Date: 2026-06-04
- Area: Editor/Render/Vulkan
- Related files: `engine/Editor/src/VulkanAdapter.cpp`, `engine/Editor/src/ViewportPanel.cpp`
- Status: Complete

## Goal

修复退出时 Vulkan validation 报告 `UNASSIGNED-non-acquired-swapchain-image-used` 的问题，并清理 viewport 初始显示上下颠倒的临时处理。

## Context

当前主窗口 swapchain 的 acquire/present 生命周期由 `VulkanRHIDevice::BeginFrame`、RenderGraph 提交和 `VulkanRHIDevice::EndFrame` 管理。RenderGraph 只应该对 `GetActiveSwapchainTexture()` 返回的已 acquire backbuffer 进行 layout transition 和渲染。

之前 Editor 打开了 `ImGuiConfigFlags_ViewportsEnable`，并在 `VulkanAdapter::EndFrame` 中调用 `ImGui::UpdatePlatformWindows()` 和 `ImGui::RenderPlatformWindowsDefault()`。该模式会让 Dear ImGui backend 为额外平台窗口创建并管理自己的 swapchain，绕过当前 RHI 的 acquire/present 流程。退出和窗口销毁时，这些平台窗口 swapchain image 可能被 backend 渲染或 transition，但并不处于当前 RHI 已 acquire 的状态，因此触发 validation。

Viewport 上下颠倒的问题上一轮临时通过 `ImGui::Image` 的 UV 翻转处理，但这会把显示层和 Vulkan 投影层的坐标约定混在一起。当前 `Camera::UpdateMatrices` 已经在投影矩阵中做了 Vulkan Y 修正，viewport 显示层不应再额外翻转。

## Changes

- 移除 `ImGuiConfigFlags_ViewportsEnable`。
- 移除 `ImGui::UpdatePlatformWindows()` 和 `ImGui::RenderPlatformWindowsDefault()` 调用。
- 移除平台窗口样式分支，避免误判多视口仍被启用。
- 将 `ViewportPanel` 的 `ImGui::Image` 恢复为默认 UV，避免显示层二次翻转。

## Verification

- Commands run: `cmake --build build`
- Result: 构建成功，`build/bin/ChikaEditor.exe` 完成链接。
- Commands run: `rg -n "ViewportsEnable|RenderPlatformWindowsDefault|UpdatePlatformWindows|ImGui::Image" engine\Editor\src`
- Result: 不再存在 ImGui 多视口和平台窗口渲染调用，仅保留 viewport 面板的 `ImGui::Image`。
- Commands run: 启动 `build/bin/ChikaEditor.exe`，等待数秒后发送正常关闭信号。
- Result: 进程已退出，没有挂住。该命令未捕获子进程 validation 输出，因此最终 validation 文本仍需在用户本地控制台观察确认。
- Remaining warnings or risks: 若以后要恢复 ImGui 多视口，需要把平台窗口 swapchain 生命周期纳入 RHI，不能直接使用 ImGui backend 的默认平台窗口渲染路径。

## Next Steps

- 为 RHI 增加 swapchain image acquired 状态断言，在提交前检测 presentable texture 是否属于当前 acquired image。
- 将 ImGui 平台多视口作为独立功能重新设计，显式接入 RHI acquire/present，而不是直接调用 `RenderPlatformWindowsDefault()`。
- 统一 `Mat4::Perspective`、`Camera`、shader clip space 和 viewport/ray 坐标约定，避免显示层临时翻转。
- 增加 RenderGraph debug dump，输出每帧写入的 swapchain texture handle 和当前 acquired image index。
