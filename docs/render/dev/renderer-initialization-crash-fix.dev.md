# Renderer 初始化崩溃修复开发日志

## Metadata

- Date: 2026-06-04
- Area: Runtime/Render/Resource
- Related files: `engine/Runtime/Render/include/ChikaEngine/Renderer.hpp`, `engine/Runtime/Render/src/renderer.cpp`
- Status: Complete

## Goal

修复 `ChikaEditor.exe` 启动后在 `AssetManager::GetMesh -> SlotMap::Get -> std::vector::size()` 路径上的访问冲突，并确认 Renderer 与资源系统的依赖注入顺序符合当前 core/resource 设计。

## Context

当前 `ResourceManager` 通过构造函数接收 `IRHIDevice&` 和 `AssetManager&`，并在 `UploadMesh`、`UploadMaterial` 等路径中直接使用这个引用访问 asset 层数据。这个设计要求 `Renderer` 在创建 `ResourceManager` 前必须已经持有有效的 `AssetManager` 指针。

本次崩溃地址显示 `std::vector::size()` 的 `this` 为空，实际原因不是 `SlotMap` 容器算法错误，而是 `Renderer::Initialize` 没有把 `RendererCreateInfo::assetManager` 写入 `m_assetMgr`，却直接用 `*m_assetMgr` 构造 `ResourceManager`。这会让 `ResourceManager` 保存一个从空指针解引用得到的无效 `AssetManager&`，后续访问 `m_meshes` 时表现为读 `0x58` 附近地址。

## Changes

- `RendererCreateInfo::assetManager` 增加默认值 `nullptr`，避免未初始化指针进入初始化流程。
- `Renderer::Initialize` 在创建 `ResourceManager` 前保存 `createInfo.assetManager` 到 `m_assetMgr`。
- `Renderer::Initialize` 对空 `assetManager` 直接抛出 `std::invalid_argument`，让错误停在依赖注入边界。
- `Renderer::Initialize` 保存 `pipelineMode`，并将初始 viewport 尺寸同步为 create info 中的窗口尺寸。
- `Renderer::Initialize` 在 RHI factory 返回空设备时抛出 `std::runtime_error`。
- 删除重复创建 `m_depthTexture` 的调用，避免初始化阶段泄漏一次 depth texture 句柄。

## Verification

- Commands run: `cmake --build build`
- Result: 构建成功，`build/bin/ChikaEditor.exe` 完成链接。
- Commands run: 临时将 `D:\Scoop\apps\python311\3.11.9` 加入 PATH 后执行 `lldb --batch`，在 `ResourceManager::_UploadMesh` 处断点检查。
- Result: `m_assetManager` 为有效对象地址，`m_meshes` 中已有 alive entry，不再是空对象基址。
- Commands run: 使用 lldb 在 `Renderer::EndFrame` 处断点运行 `build/bin/ChikaEditor.exe`。
- Result: 程序通过资源加载、上传、渲染图执行，并到达首帧 `Renderer::EndFrame()`，未复现原始访问冲突。
- Remaining warnings or risks: 构建仍有既有的 `InspectorPanel.cpp` 中 `strncpy` deprecated warning。`lldb` 默认启动缺少 `python311.dll`，需要临时修正 PATH 后才能使用。

## Next Steps

- 将 Renderer 的必要依赖改为更明确的构造期或初始化期 contract，例如 `RendererCreateInfo` 校验函数或 builder。
- 为 `RenderSubsystem` 增加构造期断言或提前返回，避免 `_assetMgr`、`_resourceMgr` 为空时继续收集 draw command。
- 将 `ResourceManager::GetMesh/GetMaterial/GetTexture` 的返回策略从直接解引用改为可诊断接口，至少在 Debug 下对无效 handle 给出明确错误。
- 梳理 `feat/render` 与 `main` 的差异，优先处理 Renderer/RHI/Resource 的所有权边界，再处理 deferred pipeline 的功能扩展。
