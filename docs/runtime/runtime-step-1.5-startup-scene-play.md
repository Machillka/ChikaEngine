# Runtime Step 1.5：加载启动 Scene 并进入 Play

## Metadata

- Status: Planned
- Depends on: Step 1.3, Step 1.4
- Suggested scope: `engine/Game`, `SceneManager`, Asset/Project integration, tests

## Goal

完成第一个真正的独立游戏启动闭环：`ChikaGame` 读取 Project、按 GUID 加载启动 Scene，并自动进入 Play Mode。

## Planned Changes

- `GameApplication` 在 EngineContext 初始化后解析 `startupScene`。
- SceneManager 增加按 Scene Asset GUID 加载并激活的高层入口。
- 加载成功后由 Game Runtime 明确调用 `StartPlayMode`。
- Game Runtime 不创建 Editor Baseline Scene，不使用 Editor Camera/Panel 流程构造内容。
- 定义启动失败错误码和日志上下文。

## Minimum Deliverable

- `ChikaGame --project <project>` 启动指定 Scene。
- Scene 中序列化的 GameObject、Component、脚本和资产引用被解析。
- Runtime 自动进入 Play，退出时执行完整 EndPlay 和 Shutdown。

## Done When

- 修改 Project 中 `startupScene` GUID 可以切换启动 Scene。
- 缺失启动 Scene、Scene 反序列化失败或依赖缺失时返回非零退出码。
- 自动测试覆盖 Scene GUID 解析和 Play 生命周期。

## Verification

- `cmake --build build --target ChikaGame`
- `ctest --test-dir build --output-on-failure`
- 启动 Game，确认 Scene lifecycle 为 Load -> Activate -> BeginPlay -> EndPlay

## Not Included

- 不要求发布态脱离源资产。
- 不实现异步加载或 Loading Screen。

## Next Step

- Step 1.6：让启动 Scene 提供正式 Runtime Camera 和 Light。

