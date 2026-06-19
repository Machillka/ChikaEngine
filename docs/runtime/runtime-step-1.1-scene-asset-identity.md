# Runtime Step 1.1：建立 Scene 资产身份

## Metadata

- Status: Complete
- Depends on: Step 0.4, existing Scene serialization
- Suggested scope: `Runtime/Asset`, `Runtime/Framework`, `Assets/Scenes`, `tests/`

## Goal

让 Scene 进入 AssetDatabase 管理，获得稳定 GUID、meta 和明确导入结果，为 Project Descriptor 使用 GUID 指定启动 Scene 建立基础。

## Planned Changes

- 增加 `AssetType::Scene` 和 Scene 文件扩展名约定。
- 增加最小 Scene Importer；首版允许验证后复制到 Imported/Cooked 中间产物，不改写 Scene 内容。
- 为 SceneManager 增加按资产记录加载 Scene 的入口。
- 增加 Scene meta、重复 GUID 和缺失 Scene 的验证。

## Minimum Deliverable

- 一个保存在 `Assets/Scenes/` 下、带 `.meta` 的 Scene。
- AssetDatabase 可以按 GUID 查询 Scene。
- SceneManager 可以通过 Scene 资产记录加载该 Scene。

## Done When

- Scene 移动且 `.meta` 同步移动后，GUID 保持不变。
- Scene 加载不要求调用方知道源文件绝对路径。
- Asset Pipeline 测试覆盖 Scene 分类和查询。

## Verification

- `ctest --test-dir build -R Chika.AssetPipeline --output-on-failure`
- 保存 Scene、重新扫描 AssetDatabase、按 GUID 加载

## Not Included

- 不实现 Scene Streaming。
- 不迁移 Scene 内部所有 Component 资产引用。

## Next Step

- Step 1.2：建立统一 AssetReference 并迁移可 Cook 的持久引用。

## Implementation Record

- `AssetType::Scene` 识别 `.scene`，默认使用独立 `SceneImporter` 校验 `Scene.GameObjects` 结构。
- `AssetDatabase::Scan()` 现在会拒绝重复 GUID，而不是静默覆盖记录。
- `SceneManager::LoadScene(AssetGuid)` 通过 AssetDatabase 解析 Scene，不要求调用方持有路径。
- 新增 `Assets/Scenes/Main.scene` 与稳定 meta；Project 启动只引用其 GUID。
- `Chika.AssetPipeline` 覆盖 Scene 分类、Importer 和重复 GUID；`Chika.SceneIntegration` 覆盖按 GUID 加载。

当前 Scene Importer 在 Development 模式仍返回源 Scene；Cooked Scene 产物由 Phase 3 实现。
