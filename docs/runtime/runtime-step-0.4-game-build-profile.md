# Runtime Step 0.4：建立 Game Build Profile

## Metadata

- Status: Complete
- Depends on: Step 0.3
- Suggested scope: Root/Engine CMake, build documentation, tests

## Goal

建立可单独选择的 Game、Editor、Tool 和 Test 构建配置，并让 Debug/Release 行为由构建类型决定，而不是全局写死。

## Planned Changes

- 增加明确 CMake Options，例如 `CHIKA_BUILD_GAME`、`CHIKA_BUILD_EDITOR`、`CHIKA_BUILD_TOOLS` 和标准 `BUILD_TESTING`。
- 仅在需要时构建 Editor、Cooker 和 Packaging Tool。
- 移除全局无条件 `CHIKA_DEBUG`，改为按 Configuration 设置调试、Validation 和诊断能力。
- 定义 Development Game 与 Release Game 的最小差异。
- 增加 Game-only 配置和构建验证。

## Minimum Deliverable

- 可以在不构建 Editor 的情况下配置并构建 `ChikaGame`。
- Release Game 不启用 Debug-only 行为和 Vulkan Validation。
- Editor Development 配置保持现有调试能力。

## Done When

- Game-only、Editor Development 和 Test 三种配置都能独立构建。
- 关闭 `CHIKA_BUILD_EDITOR` 后构建图中不存在 Editor/ImGui 目标。
- 配置选项和支持矩阵写入构建文档。

## Verification

- `cmake -S . -B build-game -DCHIKA_BUILD_GAME=ON -DCHIKA_BUILD_EDITOR=OFF -DCHIKA_BUILD_TOOLS=OFF -DBUILD_TESTING=OFF`
- `cmake --build build-game --config Release`
- 配置并构建 Editor Development。
- 配置并运行测试目标。

## Implementation

- 新增 `CHIKA_BUILD_GAME`、`CHIKA_BUILD_EDITOR`、`CHIKA_BUILD_TOOLS` 与 `CHIKA_ENABLE_RUNTIME_SMOKE_TESTS` CMake Options；测试继续使用标准 `BUILD_TESTING`。
- `engine/CMakeLists.txt` 仅在对应选项开启时加入 Game 或 Editor。
- Editor 关闭时不配置 ImGui target；`CHIKA_BUILD_TOOLS` 预留给后续 Cooker/Packaging Tool，当前没有独立 Tool target。
- 全局 `CHIKA_DEBUG` 改为只对 Debug Configuration 定义。
- `RendererCreateInfo::enableValidation` 默认随 `CHIKA_DEBUG` 切换，Release Game 默认关闭 Vulkan Validation。

## Verification Result

- Editor Development 主构建：配置和全量构建通过。
- Game-only Release：`CHIKA_BUILD_EDITOR=OFF`、`CHIKA_BUILD_TOOLS=OFF`、`BUILD_TESTING=OFF` 配置与构建通过。
- Test-only Debug：`CHIKA_BUILD_GAME=OFF`、`CHIKA_BUILD_EDITOR=OFF`、`BUILD_TESTING=ON` 配置、构建和 6/6 CTest 通过。
- Game-only `compile_commands.json` 不包含 `CHIKA_DEBUG`，构建图不包含 ImGui 或 `ChikaEditor`。
- 6 个测试全部通过；可选窗口 Smoke Test 可通过 `CHIKA_ENABLE_RUNTIME_SMOKE_TESTS=ON` 接入 CTest。

## Build Profiles

```text
Editor Development:
  CHIKA_BUILD_EDITOR=ON
  CHIKA_BUILD_GAME=ON
  BUILD_TESTING=ON

Game Release:
  CMAKE_BUILD_TYPE=Release
  CHIKA_BUILD_EDITOR=OFF
  CHIKA_BUILD_GAME=ON
  CHIKA_BUILD_TOOLS=OFF
  BUILD_TESTING=OFF
```

## Not Included

- 不实现跨平台 Toolchain 文件。
- 不完成 Cook 或 Package。

## Next Step

- Step 1.1：让 Scene 成为正式 GUID 资产。
