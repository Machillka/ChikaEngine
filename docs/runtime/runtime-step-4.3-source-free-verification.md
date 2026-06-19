# Runtime Step 4.3：建立 Source-free Standalone 验收

## Metadata

- Status: Planned
- Depends on: Step 4.2
- Suggested scope: tests, CI, release docs

## Goal

用自动化和人工 Smoke Test 证明 Package 是真实独立游戏产物，而不是依赖仓库、Editor、源资产或开发环境才能启动的伪发布目录。

## Planned Changes

- 增加 Package 内容规则扫描和 Standalone Smoke Test。
- 将 Package 复制到仓库外临时目录，从该目录启动。
- 测试期间禁止访问或临时隐藏：
  - 仓库 `Assets/`。
  - `.meta`。
  - `.venv`。
  - Editor 和 Cooker 工具。
- 记录启动、Scene 加载、BeginPlay、渲染、脚本执行和正常退出结果。
- 将 Package 验收接入 CI 的发布门禁。

## Minimum Deliverable

- 自动 Package Scan。
- 仓库外启动 Smoke Test。
- 一份发布验收报告，包含 Package 文件数量、大小、启动结果和已知限制。

## Done When

- Package 在仓库外、无源资产、无开发 Python 环境时启动成功。
- 启动 Scene、渲染、脚本和正常退出均通过。
- Package 中不存在 Editor、`.meta`、Shader 源码和绝对路径。
- 任一发布门禁失败会阻止产物被标记为可交付。

## Verification

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- 执行 Package Target。
- 复制 Package 到仓库外临时目录并运行 Standalone Smoke Test。

## Not Included

- 不保证尚未支持的操作系统和 GPU。
- 不包含安装器、自动更新或崩溃上传服务。

## Next Step

- 独立 Game Runtime 阶段完成；随后进入 Editor 创作闭环和可玩 Demo 制作。

