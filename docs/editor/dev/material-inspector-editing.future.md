# Material Inspector Editing UI

## Metadata

- Status: Implemented (2026-07-02)
- Depends on: `docs/render/dev/steps/7.1-material-parameter-edit-api.future.md`
- Area: Editor/Inspector/Renderer
- Estimate: 1-2 days

## Goal

在 Editor 的 Inspector 面板中，为选中 GameObject 的 `MeshRenderer` 增加 Material 参数编辑界面。用户可以直接修改材质字段，例如颜色、金属度、粗糙度和发光颜色，并通过 Renderer 的材质参数接口实时更新画面。

## Current Context

- `InspectorPanel.cpp` 继续通过 Reflection 自动绘制组件字段。
- `MeshRenderer` 当前暴露 mesh/material asset reference、resolved asset handle 和非持久化 runtime material override。
- `RenderSubsystem` 会把 `MeshRenderer` 的 material asset 上传为共享 `Resource::MaterialHandle`；如果存在 runtime material override，则优先提交 override handle。
- Inspector 已在 `MeshRenderer` 折叠区追加 Material 专用面板，通过 Renderer Facade 读取运行时参数列表。

## Responsibility

- Inspector 负责 UI 展示、用户输入和调用 Renderer Facade。
- Renderer/Resource 负责参数查询、类型校验和 GPU UBO 更新。
- MeshRenderer 继续只持久化 material asset reference；runtime override 只保存运行时 material handle，不写入场景资产。
- Scene dirty 语义需要区分 runtime preview 和 asset persistence。

## UI Plan

在 `MeshRenderer` 折叠面板下增加 `Material` 区域：

- Material asset reference：继续显示当前材质来源。
- Runtime Instance 提示：未编辑时显示共享材质预览；第一次修改时自动创建 per-object runtime material instance，后续修改只影响当前 GameObject。
- 参数列表：
  - `Float`：`ImGui::DragFloat`
  - `Vec2`：`ImGui::DragFloat2`
  - `Vec3`：普通向量参数使用 `ImGui::DragFloat3`
  - `Vec4`：名字包含 `Color`、`BaseColor`、`Emissive` 时使用 `ImGui::ColorEdit4`，其他使用 `ImGui::DragFloat4`
  - `Bool`：仅在 Renderer API 明确支持后使用 `ImGui::Checkbox`
- Reset 按钮：把当前参数恢复为 shader template/default material 值。
- 不可编辑状态：
  - 没有 Renderer
  - 没有 AssetManager/ResourceManager
  - MeshRenderer material asset 未 resolve
  - Material resource 上传失败
  - Shader 没有 `material` UBO 或参数未出现在 reflection 中

## Implementation Steps

1. [x] 在 `InspectorPanel.cpp` 中为 MeshRenderer 增加专用绘制函数：
   - `DrawMeshRendererMaterialPanel(Framework::MeshRenderer& meshRenderer)`
   - 保持通用 `DrawReflectedObject()` 不负责 Material 业务 UI。

2. [x] 从选中对象找到 MeshRenderer：
   - 在组件循环中检测 `Framework::MeshRenderer`。
   - 先绘制已有反射字段。
   - 再绘制 Material 参数区。

3. [x] 获取材质资源：
   - 通过 `meshRenderer.GetMaterialAsset()` 获取 asset handle。
   - 若 asset handle 未 resolved，Inspector 会调用 `MeshRenderer::ResolveAssets()` 走现有 resolve 流程。
   - 通过 `Renderer::GetOrUploadMaterial()` 获取共享 `Resource::MaterialHandle` 作为源材质。
   - 第一次参数变更前调用 `Renderer::CreateMaterialInstance()`，并写入 `MeshRenderer::SetRuntimeMaterialOverride()`。
   - Inspector 不直接写 ResourceManager 或 RHI 数据。

4. [x] 查询参数列表：
   - 调用 `renderer->GetMaterialParameters(materialHandle)`。
   - 如果返回空列表，显示 disabled 文本，不报错刷屏。

5. [x] 绘制控件并提交修改：
   - 每个控件使用 `ImGui::PushID(parameter.name.c_str())`。
   - 只有值变化时对当前 runtime material instance 调用 `renderer->SetMaterialParameter(...)`。
   - 设置失败时显示 Inspector 内错误提示，不持续重复提交失败写入。

6. [x] Dirty 语义：
   - MVP 是 runtime per-object material preview，不保存 JSON，因此不设置 `scene->IsEditing()` dirty。
   - 如果后续增加 "Apply to Material Asset"，再设置 dirty 并写回 material JSON。
   - UI 文案需要避免让用户误以为修改已持久化。

7. [x] UX 细节：
   - 对 `BaseColor` 使用颜色编辑器。
   - 对 `Metallic`、`Roughness`、`OcclusionStrength` 使用 0-1 clamp slider。
   - 对 `NormalScale` 使用 0-4 drag/slider。
   - 对 `Emissive` 保留 HDR 颜色/强度的后续扩展空间。

## Implemented Files

- `engine/Editor/include/InspectorPanel.hpp`
- `engine/Editor/src/InspectorPanel.cpp`
- `engine/Runtime/Render/include/ChikaEngine/Renderer.hpp`
- `engine/Runtime/Render/src/renderer.cpp`
- `engine/Runtime/Framework/include/ChikaEngine/component/MeshRenderer.h`
- `engine/Runtime/Framework/src/subsystem/RenderSubSystem.cpp`
- `engine/Runtime/Resource/include/ChikaEngine/ResourceLayout.hpp`
- `engine/Runtime/Resource/include/ChikaEngine/ResourceManager.hpp`
- `engine/Runtime/Resource/src/ResourceManager.cpp`
- `tests/unit/MaterialParameterTests.cpp`

## Implementation Record

- `InspectorPanel` 在 `MeshRenderer` 组件反射字段后追加 `Material` 子树，通用 Reflection Drawer 不承担材质业务逻辑。
- UI 会显示 material asset 来源，并标注未编辑前是共享材质预览，第一次编辑会创建当前 GameObject 的 runtime material instance。
- Inspector 通过 `Renderer::GetOrUploadMaterial()` 获取共享源材质；参数发生变化时通过 `Renderer::CreateMaterialInstance()` 创建独立参数 UBO，再调用 `SetMaterialParameter()`。
- `MeshRenderer` 保存非持久化 runtime material override；`RenderSubsystem` 提交 Render Proxy 时优先使用该 override。
- `ResourceManager::CloneMaterial()` 复用 shader/pipeline/texture 绑定，只创建独立 material parameter UBO；runtime instance 卸载时不会销毁共享 pipeline/shader。
- `Float`、`Vec2`、`Vec3`、`Vec4` 和 `Bool` 都有对应控件；`BaseColor`/`Emissive` 这类颜色参数使用 `ColorEdit4`。
- `Metallic`、`Roughness`、`OcclusionStrength` 使用 0-1 slider；`NormalScale` 使用 0-4 slider。
- 每个参数带 `Reset` 按钮，恢复到 Renderer/Resource 返回的 shader template 默认值。
- 上传失败会缓存失败 asset，避免每帧重复触发上传日志；用户可点击 `Retry Material Upload` 重试。
- 运行时参数修改不设置 scene dirty，不写 material JSON，不影响 Add/Remove Component 流程。

## Tests

- Editor smoke：
  - 打开 Editor，选中带 `MeshRenderer` 的对象。
  - Inspector 显示 Material 参数列表。
  - 修改 `BaseColor` 后 viewport 下一帧只有当前 GameObject 变化。
  - 选中另一个使用同材质对象时仍显示共享材质的原始参数，直到它也被单独编辑。

- Automated coverage if practical：
   - [x] `Chika.MaterialParameter` 覆盖 Renderer material Facade 在无 ResourceManager 时返回安全失败。
   - [x] `Chika.MaterialParameter` 覆盖 runtime material instance 修改不会污染共享 material CPU shadow 和 mapped UBO。
   - [ ] Inspector 参数控件分派逻辑尚未抽出可独立测试的 ViewModel。

- Regression：
  - [x] 普通反射组件字段仍走 `DrawReflectedObject()`。
  - [x] Add/Remove Component UI 不受影响。
  - [x] `MeshRenderer::SetMaterialReference()` 仍会触发 asset dirty/resolve。

## Acceptance Criteria

- [x] Inspector 能显示选中 MeshRenderer 的 material 参数。
- [x] 修改颜色或标量参数会调用 Renderer material editing API，而不是直接写 Asset 或 RHI。
- [x] UI 明确标注 runtime instance edit，不伪装成持久化资产编辑。
- [x] 修改选中 GameObject 的材质参数不会影响使用同一 material asset 的其他 GameObject。
- [x] 无效材质或缺失 renderer 时 Inspector 不崩溃。

## Verification Record

- `cmake --build build`
- `ctest --test-dir build --output-on-failure -R "Chika\.(MaterialParameter|RenderBaseline|EnvironmentResource|ShaderInterface|AssetPipeline)"`
- `ChikaEditor.exe` hidden startup smoke

## Boundaries

- 不在本步骤实现 material asset 保存。
- 不实现 undo/redo transaction。
- 不实现 drag-and-drop texture 替换。
- 不实现持久化 per-object MaterialInstance asset；当前只提供 runtime material instance preview。
- 不修改通用 Reflection Drawer 来硬塞 Material 业务逻辑。

## Recommended Order

1. 等 Step 7.1 Renderer material API 可用。
2. 添加 MeshRenderer 专用 Material 面板。
3. 接入颜色/float/vector 控件。
4. 做 Editor smoke 验证。
5. 再考虑持久化和 undo/redo。
