# Runtime Step 0.1：冻结 Standalone 边界与基线

## Metadata

- Status: Complete
- Depends on: Existing `Application`, `EngineContext`, Scene and Asset tests
- Suggested scope: `docs/runtime/`, `tests/`

## Goal

在引入 `ChikaGame` 前明确 Editor Runtime 与 Standalone Runtime 的边界，并建立可以持续验证的最小失败/成功基线。

## Planned Changes

- 记录当前 `ChikaEditor` 的模块、文件和运行目录依赖。
- 定义 Standalone 禁止依赖清单：Editor、ImGui Panel、源资产扫描、meta 生成、热重载、Shader 编译。
- 增加 Runtime 启动配置、启动 Scene 加载和退出流程的测试计划。
- 定义发布态错误策略：配置缺失、GUID 不存在和 Cooked 文件缺失必须返回非零退出码。

## Minimum Deliverable

- 一份 Standalone Runtime Contract。
- 一组后续步骤必须保持的验收命令和错误清单。
- 明确 Development、Cook 和 Packaged 三种运行模式的职责。

## Done When

- 可以回答“Standalone Runtime 可以依赖什么、禁止依赖什么”。
- 后续每一步都能映射到至少一个自动验证或 Smoke Test。
- 当前 `ChikaEditor`、构建和 5 个既有测试保持通过。

## Verification

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- 启动并正常关闭 `build/bin/ChikaEditor.exe`

## Implementation

- 新增 `runtime-standalone-contract.md`，明确 Development Game、Editor Development 和 Packaged Game 的允许依赖与失败策略。
- 新增 `Chika.RuntimeBoundary` CTest，静态扫描 `engine/Runtime/` 与 `engine/Game/`，阻止 Editor UI 或 ImGui API 进入 Standalone 边界。
- 保留现有 5 个测试并加入边界测试，形成 Phase 0 后续步骤的固定回归基线。

## Verification Result

- `cmake --build build`：通过。
- `ctest --test-dir build --output-on-failure`：6/6 通过。
- `ChikaEditor` 隐藏运行约 5 秒并通过正常关闭流程退出，退出码 `0`。

## Remaining Boundary

- Development Game 当前仍通过 `EngineContext` 使用 `Assets/`、Importer 和热重载；这是 Step 3.4 前的明确兼容路径，不代表 Packaged Runtime 已完成。

## Not Included

- 不新增可执行程序。
- 不改变 AssetManager 或 EngineContext 行为。

## Next Step

- Step 0.2：建立无 Editor 依赖的 `ChikaGame` 可执行目标。
