# Runtime Step 2.1：建立资产依赖图

## Metadata

- Status: Planned
- Depends on: Step 1.2, Step 1.6
- Suggested scope: `Runtime/Asset`, asset loaders/importers, tests

## Goal

让资产系统能够结构化回答“一个资产直接依赖哪些资产”，为 Cook 传递闭包和精确热重载建立共同基础。

## Planned Changes

- 增加 `AssetDependencyGraph`，节点使用 `GUID + optional SubAsset` 的逻辑 Asset ID，边保存依赖类型。
- Importer 或类型专用 Dependency Collector 负责解析直接依赖。
- 首版必须覆盖：
  - Scene -> Component 资产引用。
  - Material -> Shader Template、Texture。
  - Shader Template -> Shader Source/Binary。
  - Mesh/Animation 外部文件依赖。
  - ScriptComponent -> Script。
- 检测缺失依赖、自依赖和循环依赖，并保留诊断路径。

## Minimum Deliverable

- 给定任意已注册逻辑 Asset ID，可以返回稳定排序的直接依赖 Asset ID。
- 给定启动 Scene，可以输出第一层依赖及其来源字段。
- 依赖图构建不要求把 Texture、Mesh 等完整加载为 Runtime CPU 资源。

## Done When

- 依赖结果在相同输入下确定且可重复。
- 缺失依赖错误能够指出父资产和缺失 GUID。
- Asset Pipeline 测试覆盖 Scene、Material 和 Shader Template 依赖。

## Verification

- 构建固定测试资产图并比较直接依赖集合。
- 验证缺失依赖和循环依赖诊断。
- 验证移动带 meta 的资产不会改变依赖图身份。

## Not Included

- 不在本步骤复制或转换任何 Cooked 文件。
- 不实现运行时增量 Streaming。

## Next Step

- Step 2.2：从 Root Asset 生成传递依赖 Cook Manifest。
