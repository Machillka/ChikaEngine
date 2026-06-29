# Runtime Step 1.6：建立 Runtime Camera 与 Light

## Metadata

- Status: Complete
- Depends on: Step 1.5
- Suggested scope: `Runtime/Framework`, `Runtime/Render`, Scene serialization, tests

## Goal

移除 Standalone Game 对 `CreateEditorView()` 和硬编码默认光源的依赖，让启动 Scene 自己描述游戏相机和基础灯光。

## Planned Changes

- 增加最小 `CameraComponent`，支持 Primary 标记、投影参数和 Transform 驱动。
- 增加最小 `LightComponent`，首版至少支持 Directional Light。
- RenderSubsystem 将 Scene Component 增量同步为 RenderWorld View/Light Proxy。
- Editor Camera 只用于 Editor Viewport，不参与 Game Runtime 的主 View。
- 定义无 Primary Camera 的行为：Development 输出明确错误或使用显式调试 fallback；Packaged Game 默认拒绝启动。

## Minimum Deliverable

- 启动 Scene 可以序列化一个 Primary Camera 和一个 Directional Light。
- `ChikaGame` 画面完全由 Scene Camera/Light 驱动。
- `ChikaEditor` 仍可使用独立 Editor Camera 浏览 Scene。

## Done When

- 移动 Scene Camera 会改变 Standalone Game 画面。
- 删除 Primary Camera 会触发可定位的启动错误。
- RenderSubsystem 不在 Game Runtime 路径创建默认 Editor View 或硬编码光源。
- Scene 集成测试覆盖 Camera/Light 序列化和 Render Proxy 注册。

## Verification

- 启动包含 Primary Camera/Light 的测试 Scene。
- 修改 Camera Transform 并比较运行结果。
- 删除 Primary Camera，确认 Packaged/Standalone 启动失败。
- 启动 Editor，确认 Editor Viewport Camera 独立工作。

## Not Included

- 不实现多 Camera 混合、Camera Stack 或高级光源管理。
- 不实现完整 Light Editor 工具。

## Next Step

- Step 2.1：建立资产直接依赖查询。

## Implementation Record

- 新增可序列化 `CameraComponent`，由 Transform、投影参数、Primary 和 Layer Mask 构建 `RenderView`。
- 新增可序列化 `LightComponent`，首版由 Transform、颜色、强度和阴影配置构建 Directional `RenderLightProxy`。
- `RenderSubsystem` 按 Component 生命周期增量创建、更新和删除 View/Light Proxy。
- 已移除 RenderSubsystem 中的硬编码默认光源；Game 路径不再调用 `CreateEditorView()`。
- Editor 明确使用独立 Editor View，并在基准 Scene 中创建 Scene Light。
- `Assets/Scenes/Main.scene` 提供 Primary Camera、Directional Light 与 GUID 资源引用。
- `Chika.SceneIntegration` 覆盖 Camera/Light 序列化与 Proxy 构建；Game 启动会拒绝无 Primary Camera 的 Scene。

首版只支持一个主游戏视图和方向光数据；多 Camera、Point/Spot Light 与高级 Light Editor 留给后续渲染阶段。
