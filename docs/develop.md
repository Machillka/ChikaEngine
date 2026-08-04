# ChikaEngine Development Log

## Maintenance Rule

凡是修改仓库文件，都必须在本文件增加或更新一条带日期的记录。记录至少包含：

- 日期与影响模块。
- 修改了什么文件和行为。
- 为什么修改，以及关键架构判断。
- 实际执行的验证命令与结果。
- 尚未完成的范围、风险和下一步。

新记录按时间倒序排列。这里记录已经落地的事实，不替代 `docs/<module>/plan` 或阶段规划卡片。

---

## 2026-08-04 - Embedded Python Runtime Home 跨平台修复

### Metadata

- Area: CMake / Scripts / Portability / Docs
- Status: Complete

### Changes

- `CMakeLists.txt`
  - 不再通过 `Python3_STDLIB` 的固定目录层级推算 `CHIKA_PYTHON_HOME`。
  - 使用 CMake 已选定的隔离 Python 执行 `sys.base_prefix`，将真实 CPython base prefix 编译到 `ChikaScripts`。
  - 增加命令失败或空输出检查，使 Python Home 错误在 CMake 配置期即失败。

### Reason and Architecture

- macOS Homebrew Framework Python 的 `Python3_STDLIB` 为 `<base>/lib/python3.14`，原逻辑只取一层父目录，错误将 `<base>/lib` 传给 `PyConfig.home`，导致 CPython 无法定位 `encodings` 标准库模块。
- `sys.base_prefix` 由实际解释器报告，避免在 Windows `Lib`、Unix `lib/pythonX.Y` 和 macOS Framework 布局之间编码平台特例。
- 修复只改变 Scripts 模块的编译期 Python runtime 路径，不改变脚本 API、模块所有权或 Runtime 初始化顺序。

### Verification

- `cmake -S . -B build`：通过，输出 `Python Runtime Home: /opt/homebrew/opt/python@3.14/Frameworks/Python.framework/Versions/3.14`。
- `rg -n "CHIKA_PYTHON_HOME" build/build.ninja`：确认生成宏为 Python base prefix，未以 `/lib` 结尾。
- `cmake --build build --parallel 4`：通过，`ChikaScripts`、`ChikaGame` 和 `ChikaEditor` 成功重新链接。
- `./build/bin/ChikaGame --project .../ChikaProject.json --mode development --smoke-frames 3`：返回 0，日志确认 `Python Script Engine initialized successfully.`。
- `ctest --test-dir build --output-on-failure`：17/17 通过。

### Remaining Work

- Game smoke 在 Apple M4 / MoltenVK 上报告 `vkCmdWriteTimestamp` 的 `timestampValidBits == 0` validation error；该问题与 Python 修复无关，需在 RHI GPU timestamp capability gate 中单独处理。

## 2026-08-04 - Editor 固定字符缓冲区跨平台修复

### Metadata

- Area: Editor / Portability / Docs
- Status: Complete

### Changes

- `engine/editor/src/InspectorPanel.cpp`
  - 新增 `CopyToFixedBuffer` C++20 辅助函数，按目标数组容量截断 `std::string_view`，并显式写入 `\0` 终止符。
  - 替换反射字符串编辑和 GameObject 名称编辑中的两处 MSVC 专用 `strncpy_s` 调用。

### Reason and Architecture

- Apple Clang/macOS libc 不提供 MSVC Secure CRT 的三参数 `strncpy_s` 数组重载，导致 `ChikaEditor` 在 macOS 上编译失败。
- 修复仅限 Editor UI 的字符串到 ImGui 固定缓冲区边界，不改变 Runtime 模块依赖、所有权或公开 API。
- 未直接替换为 `strncpy`，避免源字符串过长时缓冲区缺少终止符。

### Verification

- `cmake --build build --parallel 4`：通过，`ChikaEditor` 成功编译并链接。
- `ctest --test-dir build --output-on-failure`：17/17 通过。
- `git diff --check`：通过。

### Remaining Work

- ImGui 编辑缓冲区仍保持 256 字节上限，过长名称会被安全截断；本次不扩展为动态字符串 ImGui adapter。
- 本次未启动图形界面进行 Editor 交互 smoke test。

## 2026-07-15 - Skybox 环境资源解析与 RenderGraph 导入

### Metadata

- Area: Asset / Resource / Render / Docs
- Related step: `docs/render/dev/steps/6.2-skybox-render-pass.future.md`
- Status: Complete（仅完成 6.2 的资源解析与导入切片）

### Goal

把 `RenderSettings::environment.skybox` 的稳定资产引用解析为可采样的 GPU Cubemap，并以唯一的 RenderGraph 逻辑资源发布到 Blackboard `Environment.Skybox`。本次不创建 Skybox shader 或 render pass。

### Changes

- `RenderSettings.hpp`
  - `EnvironmentSettings` 增加 `Asset::AssetReference skybox` 和 `fallbackColor`。
  - 配置层只保存稳定资产身份和渲染策略，不保存 RHI handle。
- `Renderer.hpp`
  - 增加 `SetEnvironmentSettings()`，配置在下一次 RenderGraph 构建时生效。
- `EnvironmentResources.hpp/.cpp`
  - 新增 `EnvironmentResourceResolver`，集中处理 Asset 加载、Resource 上传缓存、Cubemap 契约校验和 stale handle 恢复。
  - 新增 `PublishEnvironmentSkybox()`，负责发布 Blackboard 语义。
  - 状态明确区分 disabled、缺少引用、Asset 加载失败、Resource 上传失败、纹理契约错误和 ready。
- `ResourceLayout.hpp` / `ResourceManager.hpp/.cpp`
  - `TextureGPU` 保存真实 `RHI_Format`，使 RenderGraph import descriptor 不依赖猜测。
  - 增加 `TryGetTexture()`，避免热重载后解引用已经失效的 Resource handle。
- `RenderPipeline.hpp/.cpp`
  - 在 drain texture upload jobs 前解析 Skybox，保证首次加载能进入本帧 upload 队列。
  - `AddUploadPasses()` 返回 `TextureHandle -> RGTextureHandle` 映射。
  - Skybox 命中 pending upload 时复用 upload destination RG handle；缓存已经就绪时才单独 import。
- `Assets/Textures/Skybox/default-skybox.texture(.meta)`
  - 将已有六张 face 组成默认线性 Cubemap，并提供稳定 GUID，供 smoke test 和后续 Skybox pass 使用。
- `EnvironmentResourceTests.cpp`
  - 覆盖默认 Cubemap 解析、首次上传、缓存命中、ready import、stale handle 恢复、2D 纹理拒绝和 Blackboard 发布。
- `AGENTS.md`
  - 固化本开发日志的强制维护规则。
- `docs/render/dev/steps/6.1...` / `6.2...`
  - 同步 `TextureGPU` 新契约、第一段实现状态、已完成清单和剩余 Skybox pass 工作。

### Architecture

```text
EnvironmentSettings.skybox (AssetReference)
  -> EnvironmentResourceResolver
  -> AssetManager::LoadTexture()
  -> ResourceManager::UploadTexture()
  -> TextureGPU (texture/view/sampler/format/dimension)
  -> pending upload RG handle or ready texture import
  -> RenderGraphBlackboard[Environment.Skybox]
```

所有权边界：

- `EnvironmentSettings` 拥有可持久化配置。
- `AssetManager` 拥有 CPU Asset 和 `Asset::TextureHandle`。
- `ResourceManager` 拥有 `TextureGPU` 及 RHI 资源。
- `EnvironmentResourceResolver` 只缓存非 owning handle；发现 stale handle 后通过 ResourceManager 重新上传。
- RenderGraph 只拥有本帧逻辑资源关系，不销毁 ResourceManager 的 imported texture。

同帧首次上传不能把同一物理纹理导入两次。复用 upload destination RG handle 后，后续 Skybox pass 才能得到明确的 `Upload Resources -> Skybox` 写后读依赖和正确的状态转换。

### Verification

- `cmake --build build`：通过。
- `ctest --test-dir build -R "Chika\.(EnvironmentResource|RenderBaseline|RenderPhase4)" --output-on-failure`：3/3 通过。
- `ctest --test-dir build --output-on-failure`：17/17 通过。
- `clang-format --dry-run --Werror engine/Runtime/Render/include/ChikaEngine/EnvironmentResources.hpp engine/Runtime/Render/src/EnvironmentResources.cpp`：通过。
- 隐藏窗口启动 `build/bin/ChikaEditor.exe` 5 秒并正常关闭：`ExitCode=0`。

### Remaining Work

- 尚未创建 Skybox shader、UBO、pipeline 或 render pass，因此当前画面不会显示 Cubemap。
- 尚未修改 Forward/Deferred 的 HDR `LoadOp` 和 Deferred `SceneDepth` sampled usage。
- `useFallback` 和 `fallbackColor` 已进入配置契约，但实际画面回退要在 Skybox pass 接入时完成。
