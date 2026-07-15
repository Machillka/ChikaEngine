# Hierarchy 右键创建与组件添加记录

## Metadata

- Status: Implemented in this step
- Area: Editor / SceneHierarchy / Inspector
- Scope: Edit mode authoring shortcuts

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

## Changed Files

- `engine/Editor/src/SceneHierarchyPanel.cpp`
- `engine/Editor/src/InspectorPanel.cpp`

## Boundaries

- 不实现 undo/redo。
- 不实现右键删除、复制、重命名等扩展菜单。
- 不新增组件搜索框。
- 不改 Scene 序列化格式。
