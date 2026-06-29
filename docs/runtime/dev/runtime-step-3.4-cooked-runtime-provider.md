# Runtime Step 3.4：实现 Cooked Runtime Provider

## Metadata

- Status: Planned
- Depends on: Step 1.4, Step 3.3
- Suggested scope: `Runtime/Asset`, `EngineContext`, Game Runtime, tests

## Goal

让 Packaged Game 使用 Cooked Asset Registry 和 Cooked Artifact 加载资产，彻底关闭源资产数据库、Importer、meta 生成和热重载。

## Planned Changes

- 抽象资产定位边界，例如 Source Asset Provider 与 Cooked Asset Provider。
- Development/Editor Provider 使用 AssetDatabase 和 Importer。
- Cooked Provider 只加载 Registry，并按逻辑 Asset ID 定位 Runtime Artifact。
- AssetManager 在 Packaged Mode 禁止 Path Identity、扫描、导入和热重载。
- Script Runtime 从 Cooked Script Artifact 配置导入路径。

## Minimum Deliverable

- `ChikaGame --mode packaged` 可以从 Cooked Registry 加载启动 Scene 及其依赖。
- 临时隐藏或重命名源 `Assets/` 后，Packaged Game 行为不受影响。
- Package Runtime 不读取 `.meta`，不调用 Shader Compiler。

## Done When

- Packaged Mode 的所有持久资产加载都从 `GUID + optional SubAsset` 开始。
- 缺失 Registry Entry、Artifact 或类型不匹配会快速失败并输出 GUID。
- 自动测试使用只包含 Cooked Content 的临时目录运行。

## Verification

- Cook 测试项目。
- 隐藏源资产目录。
- 从 Cooked 目录启动 `ChikaGame --mode packaged`。
- 检查日志中不存在 Scan、Import、HotReload 和 ShaderCompile。

## Not Included

- 不实现运行时按需下载、DLC 或补丁。
- 不实现异步 Streaming。

## Next Step

- Step 4.1：收集完整 Package Staging 目录。
