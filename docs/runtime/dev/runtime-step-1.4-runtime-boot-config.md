# Runtime Step 1.4：建立 Runtime Boot Config

## Metadata

- Status: Complete
- Depends on: Step 1.3
- Suggested scope: `Application`, `EngineContext`, `Runtime/Project`, `AssetManager`, tests

## Goal

移除 `EngineContext` 中写死的 `Assets/` 和开发态行为，让同一套 Engine 生命周期可以根据运行模式启动 Editor、Development Game 或 Packaged Game。

## Planned Changes

- 增加明确的 `RuntimeMode`：`Editor`、`DevelopmentGame`、`PackagedGame`。
- 将 Project Descriptor 转换为只包含已验证值的 `EngineContextCreateInfo`/Boot Config。
- EngineContext 从 Boot Config 获取：
  - Content root。
  - 是否允许资产扫描、导入、meta 生成和热重载。
  - Window、Renderer、Physics 和 Scripting 配置。
- Development Game 允许读取源资产；Packaged Game 强制使用 Cooked Content。

## Minimum Deliverable

- `ChikaEditor` 保持现有开发态资产行为。
- `ChikaGame --project ... --mode development` 使用 Project 指定内容根。
- `ChikaGame --mode packaged` 不调用源资产扫描、Importer 或热重载。

## Done When

- `EngineContext` 不再固定初始化 `"Assets"`。
- Packaged Mode 若配置到源资产目录会拒绝启动。
- 三种 Runtime Mode 的配置投影和禁止能力均有测试。

## Verification

- `ctest --test-dir build --output-on-failure`
- 分别启动 Editor、Development Game 和预期失败的无 Cooked Registry Packaged Game
- 日志确认 Packaged Mode 未启动 Importer/Hot Reload

## Not Included

- 暂不完成 Cooked Asset Provider。
- 暂不完成 Package Staging。

## Next Step

- Step 1.5：加载 Project 启动 Scene 并进入 Play。

## Implementation Record

- 新增 `RuntimeMode::{Editor, DevelopmentGame, PackagedGame}` 与 `RuntimeBootConfig`。
- `EngineContextCreateInfo` 现在显式接收 Content Root、扫描、meta、Importer、热重载、默认 Scene 和 Editor View 能力。
- `AssetDatabaseCreateInfo`/`AssetManagerCreateInfo` 让 Packaged Mode 可关闭目录创建、源扫描、meta、Importer 和热重载。
- 脚本模块搜索路径改为 Project Content Root 下的 `Scripts/`，不再写死仓库 `Assets/Scripts`。
- Editor 保持开发态行为；Development Game 使用 Project Source Content；Packaged Game 使用 Cooked Content。

当前尚无 Cooked Registry，因此示例 `--mode packaged` 会在缺失 `Content/` 时快速失败；这是 Phase 3.4 前的预期行为。
