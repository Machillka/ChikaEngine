# CMake 清理记录

本次清理目标是移除当前构建不需要的包管理依赖和无效 CMake 片段，让拉取仓库后的配置流程更直接。

## 修改内容

### 根 CMake 不再依赖 vcpkg

已从根 `CMakeLists.txt` 移除：

```cmake
file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" VCPKG_CMAKE_PATH)
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_CMAKE_PATH}/scripts/buildsystems/vcpkg.cmake")
```

现在配置项目不再要求安装 vcpkg，也不再要求设置 `VCPKG_ROOT`。

当前第三方库主要来自 `engine/ThirdParty` 下的 Git submodule。系统级依赖仍然通过 CMake 原生 `find_package` 查找：

- `Vulkan`
- `OpenGL`
- `Python3`

### ThirdParty CMake 关闭无关构建项

`engine/ThirdParty/CMakeLists.txt` 已关闭 GLFW 和 fmt 的示例、测试、文档、安装目标：

- `GLFW_BUILD_EXAMPLES OFF`
- `GLFW_BUILD_TESTS OFF`
- `GLFW_BUILD_DOCS OFF`
- `GLFW_INSTALL OFF`
- `FMT_DOC OFF`
- `FMT_TEST OFF`
- `FMT_INSTALL OFF`

这不会影响项目链接 `glfw` 和 `fmt::fmt`，只是减少第三方库默认生成的无关目标。

同时移除了已经注释掉的 GLAD 接入残留，以及 `message(STATUS ${THIRD_PARTY_PATH})` 这类调试输出。

### Editor CMake 清理无效适配器残留

`engine/Editor/CMakeLists.txt` 中删除了已经注释掉的 ImGui adapter 变量和对应 source group。

原先当前 Editor CMake 中存在未定义变量引用：

```cmake
source_group(TREE "${ADAPTER_DIR}" PREFIX "Adapter/ImGui" FILES ${ADAPTER_SRCS} ${ADAPTER_HDRS})
```

这段变量定义已经被注释，且 adapter 源文件也没有加入当前目标，因此删除它可以避免 CMake 配置时产生无意义路径或 IDE 分组问题。

## 当前仍然保留的依赖

以下依赖仍然是当前默认构建需要的：

- Git submodule
- CMake 3.24+
- C++20 编译器
- `uv` 和 Python 开发环境
- Vulkan SDK
- OpenGL 开发环境

## 验证结果

已执行过独立配置验证：

```powershell
cmake -S . -B build-codex
```

结果显示 CMake 不再读取 `VCPKG_ROOT`，也不再因为 vcpkg toolchain 缺失而失败。当前验证停在 Vulkan 查找阶段：

```text
Could NOT find Vulkan (missing: Vulkan_LIBRARY Vulkan_INCLUDE_DIR)
```

这说明 vcpkg 依赖已经移除，但本机仍需要安装 Vulkan SDK 或配置 Vulkan 的库和头文件路径。

## 没有处理的旧目录

`engine/editor_old`、`engine/render_old`、`engine/resource_old` 仍保留旧 CMake 和旧源码内容。本次没有修改这些目录，因为它们没有被 `engine/CMakeLists.txt` 加入当前默认构建。

如果之后确认旧目录不再需要，可以单独做一次删除或归档，不建议混在当前构建修复里处理。
