# Runtime Step 1.2：建立持久 AssetReference 契约

## Metadata

- Status: Planned
- Depends on: Step 1.1, existing Asset GUID loading
- Suggested scope: `Runtime/Asset`, `Runtime/Framework`, serialization, tests

## Goal

将“持久资产身份”和“进程内 Runtime Handle”分离，使 Scene 和依赖收集器不需要解析任意路径字符串。

## Planned Changes

- 增加统一 `AssetReference`，首版保存 GUID、可选 SubAsset 名称和预期 AssetType。
- Source GUID 标识源资产；SubAsset 标识源资产导出的逻辑对象，例如同一 GLTF 中的 Mesh 和 Animation Clip。
- 为开发态保留可选诊断路径，但该路径不参与资产身份和 Cook 正确性。
- 迁移首批发布阻塞引用：
  - `MeshRenderer` 的 Mesh 和 Material。
  - `Animator` 的 Animation Clip。
  - `ScriptComponent` 的 Script 资产。
  - Material 到 Shader Template、Texture 和 Shader 的引用。
- AssetManager 负责把 `AssetReference` 解析成进程内 Handle。

## Minimum Deliverable

- 启动 Scene 中所有直接运行依赖都能通过 GUID 静态枚举。
- 移动 Mesh、Material、Texture、Script 或 Animation 源文件不会破坏 Scene 引用。
- Runtime Handle 不进入 Scene、Project 或 Cook Manifest 持久化格式。
- 同一 GLTF 中的 Mesh 与 Animation Clip 可以使用同一源 GUID，但通过稳定 SubAsset 区分。

## Done When

- Scene 序列化中不再依赖上述资产的源路径作为身份。
- 缺失 GUID 会输出包含所属对象、字段和 GUID 的错误。
- Scene 集成测试覆盖资产移动后引用仍可解析。

## Verification

- `ctest --test-dir build --output-on-failure`
- 移动带 meta 的测试资产后重新加载 Scene
- 静态检查发布阻塞组件不再持久化源路径身份

## Not Included

- 不要求一次迁移所有调试或 Editor-only 路径。
- 不实现 Prefab Override 和嵌套 Prefab。

## Next Step

- Step 1.3：定义 Project Descriptor。
