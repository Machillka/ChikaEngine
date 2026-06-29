# Pull 仓库后的编译与运行准备

本文档根据当前仓库的 CMake 配置整理，重点说明拉取仓库后需要初始化什么、安装什么，以及满足哪些环境后才能配置、编译和运行代码。

## 当前构建入口

项目从仓库根目录的 `CMakeLists.txt` 开始配置，然后进入 `engine/CMakeLists.txt`。

当前实际加入构建的模块是：

- `engine/ThirdParty`：项目第三方库汇总入口。
- `engine/Runtime`：引擎运行时模块集合。
- `engine/Editor`：编辑器可执行程序 `ChikaEditor`。

`engine/editor_old`、`engine/render_old`、`engine/resource_old` 下也有旧版 CMake 配置，但当前 `engine/CMakeLists.txt` 没有把这些目录加入构建。它们可以作为历史代码参考，不是当前默认编译路径。

## 必须准备的系统环境

### 1. Git 和子模块

仓库依赖 Git submodule。拉取仓库后必须初始化第三方库：

```powershell
git submodule update --init --recursive
```

也可以在首次 clone 时直接递归拉取：

```powershell
git clone --recursive <repo-url>
```

如果已经 clone 但 `engine/ThirdParty/*` 目录为空或缺文件，重新执行 `git submodule update --init --recursive`。

### 2. CMake

根 CMake 要求：

```cmake
cmake_minimum_required(VERSION 3.24)
```

因此需要安装 CMake 3.24 或更高版本，并确保 `cmake` 在 `PATH` 中。

### 3. C++20 编译器

项目启用 C++20：

```cmake
set(CMAKE_CXX_STANDARD 20)
```

Windows 推荐使用 Visual Studio 2022 的 MSVC 工具链，并安装：

- Desktop development with C++
- Windows SDK
- MSVC v143 或更新工具集

其他平台需要能完整支持 C++20 的 Clang 或 GCC。

### 4. Python 和 uv

根 CMake 会查找 `uv`，并在配置阶段执行：

```powershell
uv sync
```

随后会使用仓库根目录下 `.venv` 里的 Python，并查找：

```cmake
find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
```

因此需要安装：

- `uv`
- Python 开发环境，且需要支持嵌入开发库 `Development.Embed`

`pyproject.toml` 当前要求：

```toml
requires-python = ">=3.14"
dependencies = [
    "clang>=21.1.7",
    "jinja2>=3.1.6",
    "pybind11>=3.0.2",
]
```

配置 CMake 时 `uv sync` 会创建或更新 `.venv`，并安装反射工具需要的 Python 依赖。

注意：如果本机无法获取 Python 3.14 或 uv 无法下载依赖，CMake 配置会失败。

### 5. Vulkan SDK

`engine/ThirdParty/CMakeLists.txt` 中有：

```cmake
find_package(Vulkan REQUIRED)
```

必须安装 Vulkan SDK，并让 CMake 能找到 Vulkan。Windows 上通常需要确保安装后环境变量已生效，例如重新打开终端或 IDE。

运行 Vulkan 后端还需要显卡驱动支持 Vulkan。

### 6. OpenGL

第三方汇总 CMake 还会执行：

```cmake
find_package(OpenGL REQUIRED)
```

因此系统需要提供 OpenGL 开发库。Windows 通常由系统 SDK/驱动提供；Linux 需要额外安装 OpenGL/Mesa 开发包。

## 由 submodule 提供的第三方库

`.gitmodules` 和 `engine/ThirdParty/CMakeLists.txt` 显示，当前项目依赖这些第三方库：

- `glfw`
- `imgui`
- `fmt`
- `nlohmann/json`
- `tinyobjloader`
- `stb`
- `JoltPhysics`
- `pybind11`
- `VulkanMemoryAllocator`
- `tinygltf`

这些库不需要手动分别安装，正常情况下通过 Git submodule 拉到 `engine/ThirdParty` 后由 CMake 加入构建。

系统级必须安装的是 Vulkan SDK、OpenGL 开发环境、CMake、C++ 编译器、uv/Python。

`.gitmodules` 中还保留了 `ImGuizmo`、`volk`、`Vulkan-Headers` 等子模块，但当前默认 CMake 构建没有把它们加入 `ChikaThirdParty`，也没有被当前 `engine/Runtime` 和 `engine/Editor` 构建路径使用。它们不是当前默认编译的必需项。

## 配置与编译

推荐从仓库根目录执行：

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

如果使用 Visual Studio 生成器，可以明确指定：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

配置阶段会自动做这些事情：

- 执行 `uv sync` 创建或同步 `.venv`。
- 使用 `.venv` 中的 Python 作为 `Python3_EXECUTABLE`。
- 在 Windows 上尝试复制 Python 运行时 DLL 到 `build/bin`。
- 扫描带 `MCLASS` 的头文件并生成反射代码。
- 生成 `ReflectionRegistry.cpp` 和 `PythonRegistry.cpp`。

## 运行

默认编辑器目标是：

```text
ChikaEditor
```

编译后可执行文件会输出到：

```text
build/bin
```

Windows Debug 构建后通常运行：

```powershell
.\build\bin\ChikaEditor.exe
```

`engine/Editor/CMakeLists.txt` 里设置了 Visual Studio 调试工作目录：

```cmake
VS_DEBUGGING_WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
```

因此从 IDE 启动时工作目录应是仓库根目录。命令行运行时也建议在仓库根目录执行，避免资源路径相对目录不一致。

## 常见失败点

### 找不到 uv

根 CMake 使用：

```cmake
find_program(UV_EXECUTABLE uv REQUIRED)
```

如果失败，安装 uv 并确认：

```powershell
uv --version
```

### uv sync 失败

CMake 配置会直接执行 `uv sync`。如果网络不可用、Python 版本不满足 `>=3.14`，或依赖解析失败，配置会中断。

可以先在仓库根目录手动执行：

```powershell
uv sync
```

### 找不到 Python Development.Embed

项目嵌入 Python：

```cmake
find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
```

如果只安装了普通解释器但没有开发库，CMake 可能找不到 `Python3_INCLUDE_DIRS` 或 `Python3_LIBRARIES`。

### 找不到 Vulkan

`find_package(Vulkan REQUIRED)` 失败时，安装 Vulkan SDK，并重新打开终端或 IDE 让环境变量生效。

### 子模块缺失

如果出现 `add_subdirectory` 指向的第三方目录不存在，或找不到 `glfw`、`fmt`、`Jolt` 等目标，通常是 submodule 没有初始化：

```powershell
git submodule update --init --recursive
```

## 当前 CMake 模块依赖概览

当前 Runtime 汇总目标是 `ChikaRuntime`，它链接以下模块：

- `ChikaCore`
- `ChikaAsset`
- `ChikaPlatform`
- `ChikaRender`
- `ChikaResource`
- `ChikaInput`
- `ChikaTime`
- `ChikaPhysics`
- `ChikaFramework`
- `ChikaScripts`

多数 Runtime 模块都会链接 `ChikaThirdParty` 和 `ChikaCore`。其中：

- `ChikaAsset` 依赖 `ChikaCore`。
- `ChikaRender` 依赖 `ChikaCore`、`ChikaAsset`、`ChikaResource`。
- `ChikaResource` 依赖 `ChikaCore`、`ChikaAsset`、`ChikaRender`。
- `ChikaFramework` 依赖 `ChikaCore`、`ChikaRender`、`ChikaResource`、`ChikaPhysics`。
- `ChikaPhysics` 使用 Jolt，并启用反射生成。
- `ChikaCore`、`ChikaPhysics`、`ChikaFramework` 会调用 `reflection_generator`。

最终 `ChikaEngine` 静态库链接 `ChikaRuntime`，`ChikaEditor` 可执行程序链接 `ChikaEngine` 和 `ChikaRuntime`。
