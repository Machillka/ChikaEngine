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
