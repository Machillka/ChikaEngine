# Hierarchy 右键创建与组件添加步骤卡片

## Metadata

- Status: Complete（延迟提交、失败回滚、自动展开和测试已实现）
- Area: Editor / SceneHierarchy / Inspector
- Scope: Edit mode authoring shortcuts
- Last reviewed: 2026-07-16

## Goal

在 Editor 面板中补齐基础 authoring 入口：

- Hierarchy 空白区域右键可以新增空 GameObject。
- Hierarchy 中右键选中 GameObject 后可以添加常用组件。
- Inspector 的 `Add Component` 弹窗同步支持 `LightComponent`。

## Minimal Implementation

本次只复用现有 Scene/GameObject/Component API：

1. 空白区域右键打开 `HierarchyContextPopup`。
2. `Create GameObject` 调用 `Scene::CreateGameobject("GameObject")`，并选中新对象。
3. GameObject 节点右键打开对象上下文菜单。
4. 对象菜单提供：
   - `Create Child`
   - `Add Component/MeshRenderer`
   - `Add Component/Light`
   - `Add Component/Animator`
   - `Add Component/Rigidbody`
   - `Add Component/ScriptComponent`
5. 新增 child 后通过 `Transform::SetParent(parent, true)` 保持世界变换。
6. 所有编辑操作只在 `scene->IsEditing()` 时可用，并设置 `EditorContext::isDirty`。

## Resolved Gap - Create Child 无法可靠使用

### Symptom

- 点击 `Create Child` 后 child 可能不出现在 Hierarchy 中。
- 父节点没有自动展开时，新对象即使已经创建，也会表现为“没有反应”。
- 重复创建可能出现 Hierarchy 刷新异常；当前入口没有输出失败原因。

### Root Cause

`SceneHierarchyPanel::OnImGuiRender()` 使用 range-for 遍历 `Scene::GetAllGameobjects()`，而 `DrawGameObjectNode()` 在菜单回调中立即调用 `Scene::CreateGameobject()`。后者向同一个 `_gameobjects` `std::vector` 执行 `emplace_back()`，可能触发扩容并使正在使用的遍历迭代器失效，后续行为属于未定义行为。

另外存在三个可观测性缺口：

- `Transform::SetParent()` 的 `bool` 返回值被忽略，挂载失败仍会选中 child 并标记 dirty。
- 创建成功后没有记录“下一帧展开父节点”的 UI 状态，child 可能隐藏在折叠节点中。
- 操作只在 Edit Mode 可用，但 Play/Pause Mode 下入口直接消失，没有解释禁用原因。

底层 hierarchy API 不是本问题根因：`ChikaSceneIntegrationTests` 已覆盖同 Scene 的创建、合法 parent 挂载和循环拒绝，当前直接运行退出码为 0。

## Implemented Fix

### Goal

把 Hierarchy 的结构修改从 ImGui 树遍历中移出，形成“绘制阶段记录命令，遍历结束后提交命令”的最小闭环。

### Implemented Changes

1. `SceneHierarchyPanel` 增加待处理的 Create Child 请求，只保存稳定的 parent `GameObjectID`，不缓存 `GameObject*` 或 `Transform*`。
2. `Create Child` 菜单点击时仅写入请求；当前帧继续完成 Hierarchy 遍历，不修改 `Scene::_gameobjects`。
3. 根节点遍历结束后通过 `HierarchyActions::CommitCreateChild()` 重新解析 parent，再创建和挂载 child。
4. 只有创建和挂载均成功时才：
   - 选中新 child；
   - 设置 `EditorContext::isDirty`；
   - 记录 parent ID，下一帧使用 ImGui tree open state 自动展开父节点。
5. 创建成功但挂载失败时通过 `Scene::DestroyGameObject()` 回滚新对象，保留原 selection，并输出明确的 Editor 日志。
6. Play/Pause Mode 保持禁止结构编辑；对象菜单、顶部按钮和空白区域菜单均显示禁用入口及 Edit Mode 原因。

### Architecture

```text
ImGui node context menu
  -> enqueue CreateChildRequest(parentId)
  -> finish Scene::_gameobjects traversal
  -> resolve parentId from active Scene
  -> create child
  -> SetParent(parent, keepWorldTransform=true)
  -> success: selection + dirty + expand parent
  -> failure: rollback + diagnostic
```

延迟命令只解决 Editor 绘制期间的容器失效问题，不改变 Scene、Transform、序列化或运行时 hierarchy 契约。

### Acceptance Criteria

- 在空父节点、已有 child 的父节点和多层父节点上执行 `Create Child`，child 均立即挂载到正确 parent。
- 创建期间不在 `Scene::GetAllGameobjects()` 遍历体内修改 `_gameobjects`。
- 创建后父节点自动展开，新 child 可见且成为当前 selection。
- `child->transform->GetParent()`、parent `GetChildren()` 和序列化 parent ID 一致。
- 创建或 `SetParent()` 失败时不设置 dirty、不改变 selection、不留下 orphan，并产生可定位日志。
- 连续创建至少 50 个 child 不崩溃、不丢失节点、不产生重复 hierarchy 引用。
- Play/Pause Mode 无法执行结构修改，并能看见明确原因。
- `ChikaSceneIntegrationTests` 继续通过；补充针对延迟 Create Child 提交流程的最小 Editor/纯逻辑测试，或记录无法自动化的 UI 验收步骤。

### Risk

- 如果延迟请求仍保存裸指针，Scene 切换或对象删除后仍可能悬空，因此请求必须只保存 ID，并在提交时重新解析。
- 回滚会进入 Scene 的销毁流程，验收时必须确认不会在同帧残留 pending object 或错误 selection。
- 自动展开应只影响目标 parent 一次，不能每帧强制覆盖用户的折叠选择。

### Verification Result

- `ChikaHierarchyActionTests` 覆盖无效 parent 不创建 orphan、连续创建 50 个 child、唯一 ID、双向 parent/children 关系、序列化 parent ID 和 Play Mode 拒绝修改：退出码 0。
- `ChikaSceneIntegrationTests`：退出码 0，既有 Scene/Transform hierarchy 契约未回归。
- `cmake --build build --target ChikaEditor ChikaHierarchyActionTests ChikaSceneIntegrationTests`：通过，Editor 和测试均链接成功。
- 隐藏窗口启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。
- `git diff --check`：通过，仅有工作区既有的 LF/CRLF 转换提示。
- UI 自动展开与禁用提示由 `SceneHierarchyPanel` 状态路径和 Editor 构建覆盖；仍建议在提交前进行一次人工右键交互 smoke。

## Changed Files

- `engine/Editor/src/SceneHierarchyPanel.cpp`
- `engine/Editor/src/InspectorPanel.cpp`

本次修复新增/修改：

- `engine/Editor/include/HierarchyActions.hpp`
- `engine/Editor/src/HierarchyActions.cpp`
- `engine/Editor/include/SceneHierarchyPanel.hpp`
- `engine/Editor/src/SceneHierarchyPanel.cpp`
- `tests/unit/HierarchyActionTests.cpp`
- `tests/CMakeLists.txt`

## Boundaries

- 不实现 undo/redo。
- 不实现右键删除、复制、重命名等扩展菜单。
- 不新增组件搜索框。
- 不改 Scene 序列化格式。
- 不在本卡片引入通用 undo/redo command framework；这里只保留面板内最小延迟请求。
