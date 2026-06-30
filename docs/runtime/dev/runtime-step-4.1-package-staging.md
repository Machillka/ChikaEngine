# Runtime Step 4.1：建立 Package Staging

## Metadata

- Status: Planned
- Depends on: Step 3.4
- Suggested scope: Build/Packaging tools, CMake, deployment tests

## Goal

将 Game 可执行程序、Project Runtime 配置、Cooked Content 和必要运行库收集到一个可复制的 Staging 目录。

## Planned Changes

- 定义 Package Layout，例如 `dist/<project>/<platform-config>/`。
- 收集：
  - `ChikaGame.exe`。
  - 发布态 Project Descriptor。
  - Cooked Asset Registry 和 Content。
  - CPython Runtime、匹配的标准库和 Cooked Script Artifact。
  - 实际需要的动态运行库。
- 排除：
  - Editor 可执行程序和 Editor 配置。
  - `Assets/`、`.meta`、Shader 源码、Importer 工具、构建目录和测试文件。
- 生成 Package Manifest，记录文件相对路径、Hash 和大小。

## Minimum Deliverable

- 一个目录结构完整、可复制的 Staging Package。
- Package Manifest 可用于检查缺失、意外文件和发布内容变化。
- Game 工作目录设为 Package Root 时能够定位 Project 与 Content。

## Done When

- Package 不依赖仓库工作目录或 `.venv`。
- Python 脚本运行不依赖开发机 Python 安装。
- 扫描 Package 不包含禁止文件和绝对路径。

## Verification

- 从空 Staging 目录重新收集全部文件。
- 校验 Package Manifest 中每个 Hash。
- 在 Staging Root 直接启动 Game。

## Not Included

- 不生成安装程序、压缩包或签名。
- 不处理平台商店发布。

## Next Step

- Step 4.2：建立一键可重复 Package Target。

