# Runtime Step 1.3：定义 Project Descriptor

## Metadata

- Status: Planned
- Depends on: Step 1.1, Step 1.2
- Suggested scope: `Runtime/Project`, project root, tests

## Goal

引入类似 Unreal `.uproject` 的轻量 Project Descriptor，作为项目身份、启动 Scene 和游戏级配置的唯一入口。

## Planned Changes

- 增加 `ProjectDescriptor` 数据结构、JSON Loader 和版本校验。
- Descriptor 使用相对 Project Root 的路径和稳定 GUID，不保存机器绝对路径。
- 首版字段仅覆盖独立运行必要配置：
  - Project name、format version。
  - Content root、cooked content root。
  - Startup Scene GUID。
  - Window、render pipeline、physics fixed step、scripting 开关。
  - `alwaysCook` Root Asset 列表。
- 定义配置优先级：引擎默认值 < Project Descriptor < 命令行覆盖。

## Minimum Deliverable

下面示例只描述首版最小字段；代码实现必须通过结构化 Loader 读取，而不是在 Runtime 中手工查 JSON 字段。

```json
{
  "version": 1,
  "name": "ChikaSampleGame",
  "contentRoot": "Assets",
  "cookedContentRoot": "Content",
  "startupScene": "122515072b7d31c965a64a1eba21da4e",
  "alwaysCook": [],
  "window": {
    "title": "Chika Sample Game",
    "width": 1280,
    "height": 720,
    "fullscreen": false
  },
  "runtime": {
    "renderPipeline": "forward",
    "fixedDeltaTime": 0.016666667,
    "maxPhysicsStepsPerFrame": 4,
    "enableScripting": true
  }
}
```

## Done When

- `ChikaGame --project <path>` 可以加载并验证 Descriptor。
- 缺失版本、项目名、内容根或启动 Scene 时快速失败。
- Descriptor 移动到另一台机器后不需要修改绝对路径。
- Project Loader 有独立单元测试。

## Verification

- 加载合法 Project Descriptor。
- 验证缺失字段、未知版本、非法 GUID 和非法相对路径。
- 验证命令行覆盖不会修改 Project 文件。

## Not Included

- 不实现 Editor 项目向导。
- 不实现插件列表、平台配置矩阵或多启动地图。

## Next Step

- Step 1.4：将 Project 配置投影到 EngineContext。

