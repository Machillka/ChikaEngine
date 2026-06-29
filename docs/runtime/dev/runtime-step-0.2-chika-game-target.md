# Runtime Step 0.2：建立 ChikaGame 可执行目标

## Metadata

- Status: Complete
- Depends on: Step 0.1
- Suggested scope: `engine/Game/`, `engine/CMakeLists.txt`, `tests/`

## Goal

新增只依赖 `ChikaEngine`/`ChikaRuntime` 的独立游戏入口，证明主循环和 Runtime 模块不需要 Editor 才能运行。

## Planned Changes

- 新增 `ChikaGame` CMake target 和最小 `GameApplication`。
- `GameApplication` 复用 `Application::Run`，不创建 `EditorManager`、ImGui Adapter 或 Panel。
- 临时允许通过命令行指定开发态内容根目录，用于后续 Project Descriptor 接入前的 Smoke Test。
- 增加链接边界检查，禁止 `ChikaGame` 链接 `engine/Editor`。
- 本步骤允许 Runtime 暂时保留已有 ImGui 扩展接口，但 `ChikaGame` 不初始化或提交 Editor UI。

## Minimum Deliverable

- `build/bin/ChikaGame.exe`。
- 空 Scene 可以初始化、进入主循环并正常关闭。
- `ChikaEditor` 和 `ChikaGame` 使用同一套 Engine 生命周期。

## Done When

- `ChikaGame` 能独立启动并退出，且日志中没有 Editor 或 ImGui Context 初始化。
- 静态依赖检查确认 `ChikaGame` 不包含 Editor 源文件和链接目标。
- Runtime 初始化失败时返回非零退出码。

## Verification

- `cmake --build build --target ChikaGame`
- 启动 `build/bin/ChikaGame.exe`，运行数秒后正常关闭
- 检查链接目标和启动日志中不存在 Editor 初始化

## Implementation

- 新增 `engine/Game/` 和 `ChikaGame` 可执行目标，只链接 `ChikaEngine`。
- 新增最小 `GameApplication`，复用 `Application::Run()` 与 `EngineContext` 生命周期，不创建 Editor、Panel 或 UI Backend。
- 新增 `--smoke-frames N`，通过正常 `RequestExit()` 路径自动退出，便于验证初始化、主循环和逆序关闭。
- Runtime 初始化或执行异常继续由 `Application::Run()` 返回非零退出码。

## Verification Result

- Debug 与 Release `ChikaGame.exe --smoke-frames 3` 均正常退出，退出码 `0`。
- 启动日志不包含 Editor 或 UI Backend 初始化。
- `engine/Game/` 静态边界检查未发现 Editor/ImGui 引用。

## Remaining Boundary

- 未实现命令行内容根目录，因为正式内容根应由 Step 1.3/1.4 的 Project Descriptor 与 Runtime Boot Config 统一提供，避免新增临时配置入口。

## Not Included

- 暂不加载 Project Descriptor。
- 暂不加载启动 Scene。
- 暂不 Cook 或 Package。

## Next Step

- Step 0.3：将 ImGui/Editor UI 从通用 Render/RHI 接口外移。
