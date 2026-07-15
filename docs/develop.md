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

## 2026-07-15 - 修复天空盒视角移动时的挤压缩放

### Metadata

- Area: Render / Skybox / Math Contract / Test / Docs
- Related step: `docs/render/dev/steps/6.2-skybox-render-pass.future.md`
- Status: Complete

### Goal

修复相机转动或移动观察方向时，天空盒纹理在屏幕边缘发生挤压、缩放的问题，同时保持相机平移不影响天空方向。

### Cause

`MakeSkyboxInverseViewProjection()` 把 `Mat4::Inverse()` 的转置结果直接当作数学 inverse 返回，`UpdateSkyboxData()` 又按 row-major -> GLSL column-major 契约转置一次。最终 Shader 收到 inverse-transpose，而不是 inverse view-projection。中心射线仍接近正确，但边缘射线会错误包含 projection 的 near/far 项，因此旋转观察时产生非刚性形变。

### Changes

- `RenderPipelinePasses.cpp` 在 Skybox 局部恢复真正的数学 inverse，再交给既有 UBO 上传转置；没有全局修改 `Mat4::Inverse()`，避免扩大到 Transform、Gizmo 等调用方。
- `RenderGraphSkyboxPassTests.cpp` 修正测试 FOV 的弧度输入和 Vulkan Y flip，使测试矩阵与 Runtime 一致。
- 新增屏幕边缘射线测试：验证方向符合 FOV/aspect，并且改变 near clip 不改变天空采样方向。
- Shader、Cubemap 资源、采样器和 RenderGraph pass 顺序均不需要修改。

### Architecture

```text
CPU row-major projection * rotation-only view
  -> mathematical inverse
  -> one upload transpose
  -> GLSL column-major inverse view-projection
  -> edge ray depends only on FOV/aspect/rotation
```

### Verification

- 修复前 `Chika.RenderGraphSkyboxPass` 新回归测试按预期失败：edge ray 不匹配 FOV/aspect，且依赖 near/far clip。
- 局部修复后同一测试通过。
- `cmake --build build`：通过；Game、Editor、Benchmark、Runtime 与测试 target 均完成链接。
- `ctest --test-dir build --output-on-failure -R "Chika\.(RenderGraphSkyboxPass|EnvironmentProjection|EnvironmentResource|RenderBaseline|ShaderInterface|ProjectDescriptor)$"`：6/6 通过。
- `ChikaGame.exe --smoke-frames 120 --project ChikaProject.json`：退出码 0；NightSky 异步转换、RGBA16F Cubemap 上传和 environment ready 均正常，未出现 Vulkan 错误。
- 相关 C++ 文件执行 `clang-format --dry-run --Werror`：通过。

### Remaining / Next

- `Mat4::Inverse()` 的全局命名与返回存储契约仍值得独立审计，但不属于本次 Skybox 最小修复范围。

---

## 2026-07-15 - NightSky 默认天空盒与非阻塞环境加载

### Metadata

- Area: Asset Jobs / Render Environment / Project / Assets / Test / Docs
- Related step: `docs/render/dev/steps/6.2a-hdr-exr-skybox-import.future.md`
- Status: Complete

### Goal

删除 Step 6.2A 为验收生成的程序化天空盒样例，默认运行配置改用真实 `NightSkyHDRI008_2K_HDR.exr`；同时消除同步 EXR decode 和 equirectangular projection 对首帧的阻塞。

### Changes

- 新增 `NightSkyHDRI008_2K_HDR.texture`，以稳定 GUID 引用 2048x1024 OpenEXR，输出 512x512x6 RGBA16F。
- `ChikaProject.json` 的 `alwaysCook` 与 `runtime.environment.skybox` 均切换到 NightSky descriptor。
- 删除 `procedural-studio.hdr/.exr`、两个程序化 descriptor、`ChikaProject.exr.json` 和 `ChikaEnvironmentSampleGenerator`；保留通用 `CHIKA_BUILD_TOOLS` 开关。
- `EnvironmentResourceResolver` 新增 `Loading` 状态，并通过现有 `AssetManager::LoadTextureAsync` 调度 EXR decode/projection。
- loading 期间不触发失败 fallback，也不发布半成品 Texture；Renderer 使用 `fallbackColor` 继续首帧。future ready 后，主线程再执行 ResourceManager/RHI upload。
- EnvironmentResourceTests 新增单 worker gate 测试，确定性证明 Resolver 在 worker 被占用时立即返回，之后能够转为 Ready。

### Architecture

```text
main/render thread                     asset worker
------------------                     ------------
Resolve environment
  -> schedule async load ------------> EXR decode
  <- Loading immediately               equirectangular projection
render fallbackColor                   create CPU TextureCube
next frame polls future <------------- ready handle
ResourceManager/RHI upload
publish Environment.Skybox
```

CPU 像素工作可以并行，但 RHI resource creation/upload 仍保持在渲染主线程。这既消除启动卡顿，又不把 Vulkan 对象生命周期扩散到 Asset worker。

### Verification

- `cmake --build build --target ChikaEnvironmentResourceTests ChikaProjectDescriptorTests ChikaGame`：通过。
- `ctest --test-dir build --output-on-failure -R "Chika\.(EnvironmentResource|ProjectDescriptor)$"`：2/2 通过。
- `cmake --build build`：通过；Game、Editor、Benchmark、Runtime 与全部测试 target 均完成编译链接。
- `ctest --test-dir build --output-on-failure -R "Chika\.(AssetJobs|AssetPipeline|TextureLoader|EnvironmentProjection|EnvironmentResource|RenderGraphSkyboxPass|ProjectDescriptor|ShaderInterface)$"`：8/8 通过。
- 相关 C++ 文件执行 `clang-format --dry-run --Werror`：通过。
- `ChikaGame.exe --smoke-frames 180 --project ChikaProject.json`：通过；首帧立即记录 asynchronous loading，NightSky 在 worker 上约 938 ms 转换为 512x512x6 RGBA16F/12582912 bytes，后续帧上传并记录 environment ready。

### Remaining / Next

- 首次加载仍消耗约 938 ms 后台 CPU 和 12 MiB staging，但不再阻塞窗口或首帧。
- 后续 Cooker 可缓存转换后的 Cubemap，进一步消除每次冷启动的后台转换成本。

---

## 2026-07-15 - Step 6.2A HDR/EXR Skybox 导入与实际采样完整实现

### Metadata

- Area: Asset / AssetDatabase / Resource / RHI / Render Environment / Project / Tools / Assets / Test / Docs
- Related step: `docs/render/dev/steps/6.2a-hdr-exr-skybox-import.future.md`
- Status: Complete

### Goal

让 `.texture` descriptor 可以把六面 HDR/EXR 或单张 2:1 equirectangular HDR/EXR 统一转换为浮点 TextureCube，并由 Project 配置、RenderGraph 和现有 Skybox Pass 实际采样。转换必须保留大于 `1.0` 的线性亮度，且不能把投影转换放入逐帧渲染路径。

### Changes

- Asset projection：
  - 新增 `EnvironmentProjection.hpp/.cpp`，按 `px,nx,py,ny,pz,nz` 与 Vulkan cube sampling 约定生成方向。
  - direction 通过 `atan2/asin` 映射到 lat-long UV；U wrap、V clamp，重采样使用双线性过滤。
  - 支持 Float16/Float32，保持源 storage，不执行 exposure、tone mapping、gamma 或 clamp-to-1。
  - 2:1 比例允许一像素容差；拒绝空 payload、NaN/Inf、LDR/sRGB 输入、非法尺寸和超过 16384 的 face。
- Descriptor 与 dependency：
  - `.texture` 新增 `projection=none|equirectangular` 与 `outputFaceSize`；equirectangular 只允许 HDR/EXR 单源。
  - `TextureData` 保存 projection 与 source path 诊断信息。
  - `AssetRecord` 保存 descriptor 的 source/cubeFaces dependency 和 write time；依赖变化会强制 reload descriptor，meta 同步写入 dependency 列表，供 Cooker 后续递归展开。
- Resource/RHI：
  - `RHICapabilities` 增加 2D/Cubemap 最大尺寸，Vulkan 从 `VkPhysicalDeviceLimits` 填充。
  - ResourceManager 在创建 GPU image 前校验 active RHI limit，并用独立状态区分尺寸超限。
  - 投影结果继续走既有 `Upload Resources`、6-layer cube view 和 Blackboard `Environment.Skybox`，Shader 无 equirectangular 分支。
- Project、示例与诊断：
  - 默认 `ChikaProject.json` 改为稳定 GUID 的 4096x2048 Radiance HDR descriptor，输出 1024x1024x6 RGBA16F。
  - 新增 `ChikaProject.exr.json`，用于 1024x512 OpenEXR -> 256x256x6 RGBA16F 的独立 smoke。
  - 新增只依赖 `stb + tinyexr` 的 `ChikaEnvironmentSampleGenerator`；示例完全程序化生成，采用 CC0-1.0，不包含外部图像数据。
  - 样例包含云层、地平线、方向灯、下半球棋盘与方向校准线，避免再次用平滑纯色素材误判清晰度。
  - 日志区分 primary HDR/EXR、packaged LDR fallback 和 fallbackColor，并记录转换耗时、输出格式与显存字节数。
- Tests：
  - 新增 `Chika.EnvironmentProjection`，覆盖六面中心方向、经线 wrap、HDR 值、非法比例、NaN 和尺寸限制。
  - 扩展 TextureLoader、AssetPipeline、EnvironmentResource、ProjectDescriptor 与 RenderGraphSkyboxPass，覆盖 descriptor、dependency hot reload、float cube staging、RHI limit、项目切换和 Blackboard 元数据。

### Architecture

```text
ChikaProject(.exr).json
  -> stable .texture AssetReference
  -> AssetDatabase dependency graph / hot reload
  -> HDR(stbi_loadf) or EXR(TinyEXR) linear 2D payload
  -> EnvironmentProjection (load/reload only)
     direction -> lat-long UV -> wrap/clamp bilinear
  -> Float16/Float32 TextureCube, six tight layers
  -> ResourceManager RHI-limit validation + staging upload
  -> Upload Resources RG handle / cached import
  -> Blackboard Environment.Skybox
  -> existing samplerCube Skybox Pass
  -> HDRSceneColor -> exposure / ACES / display gamma
```

Asset 层拥有投影算法和 CPU payload；RHI 只报告能力并创建资源；RenderPipeline 始终消费 TextureCube。这样 Project、Forward/Deferred 和 CPU/GPU-driven 不需要知道源文件是 six-face、HDR 还是 EXR。

### Verification

- `cmake --build build`：通过，Game、Editor、Benchmark、Runtime、Tests 和工具均完成链接。
- `ctest --test-dir build --output-on-failure -R "Chika\.(EnvironmentProjection|TextureLoader|AssetPipeline|EnvironmentResource|RenderGraphSkyboxPass|ProjectDescriptor)$"`：6/6 通过。
- `ChikaGame.exe --smoke-frames 2 --project ChikaProject.json`：通过；HDR 转换约 3.0 秒，输出 1024x1024x6 RGBA16F、50331648 bytes，资源状态 ready。
- `ChikaGame.exe --smoke-frames 2 --project ChikaProject.exr.json`：通过；最终 EXR smoke 转换约 185 ms，输出 256x256x6 RGBA16F、3145728 bytes，资源状态 ready。
- ChikaEditor Forward/Deferred x CPU/GPU-driven：4/4 smoke 通过，四份日志均确认 float Cubemap upload 和 environment ready，未发现 `VUID`、Validation Error 或 `VK_ERROR`。
- 可见 Editor 截图确认方向校准线随天空背景显示，画面不是 default LDR fallback 或 fallbackColor；rotation-only matrix 单测继续证明平移不改变天空、旋转改变采样方向。
- 本机没有 `renderdoccmd`，因此未生成 `.rdc`；真实 Vulkan 创建日志、六层 RGBA16F metadata、staging 值、Validation 日志与可见截图已完成等价集成验收。

### Remaining / Next

- Step 6.2A 范围内没有剩余代码项。
- 1024 face 的 Debug 首次转换约 3 秒且占 48 MiB；Step 6.3 或 Cooker 阶段应缓存转换产物，避免每次冷启动重算。
- 下一步进入 Step 6.3：Irradiance、Specular Prefilter 与 BRDF LUT；普通 Skybox mip 不能冒充 roughness-dependent prefilter。

---

## 2026-07-15 - Step 6.1A HDR/EXR 浮点纹理契约完整实现

### Metadata

- Area: Asset / Resource / RHI / Render Environment / Test / Docs
- Related step: `docs/render/dev/steps/6.1a-hdr-exr-texture-contract.future.md`
- Status: Complete

### Goal

让 Radiance `.hdr` 与 OpenEXR `.exr` 从文件解码到 Resource staging 的整个链路保留 linear float 数据和大于 `1.0` 的亮度，同时保持现有 LDR 纹理路径不回归。本步骤自身不实现单张 equirectangular 环境图转 Cubemap，也不修改 Skybox Shader；后续 Step 6.2A 已在同日完成该闭环。

### Changes

- `engine/Runtime/Asset/include/ChikaEngine/AssetLayouts.hpp`
  - 新增后端无关的 `TexturePixelStorage`（`UNorm8/Float16/Float32`）、`TextureSourceEncoding` 与 `TextureLoadStatus`。
  - `TextureData` 增加 `rowBytes`、`layerBytes`，并提供 overflow-safe 的 tight payload 计算和总字节校验。
- `engine/Runtime/Asset/src/TextureDecoder.hpp/.cpp`、`TextureLoader.cpp/.hpp`
  - LDR 继续走 `stbi_load`；Radiance HDR 改用 `stbi_is_hdr/stbi_loadf`，不再经过 8-bit 中间缓冲。
  - 使用正式 TinyEXR v3 API 解码单 part、flat scanline/tiled RGB(A) EXR；缺失 alpha 填 `1.0`，拒绝 deep、multipart、不支持的 channel/layout/compression。
  - `format=auto|rgba16f|rgba32f` 决定 payload storage；HDR/EXR `auto` 为 Float16。有限负值保留，NaN/Inf 拒绝，超出 Float16 范围时提示改用 `rgba32f`。
  - 浮点 texture 的 `srgb=true`、未实现的 `generateMips=true/mipLevels>1`、六面尺寸不一致与 LDR/HDR/EXR 混合编码均明确拒绝。
- `AssetDatabase.cpp`、`AssetManager.hpp/.cpp`
  - `.exr` 正式分类为 Texture；AssetManager 保存最近一次详细纹理加载状态，供环境资源状态映射使用。
- `RHIDesc.hpp`、`RenderResourceRequest.hpp`、`ResourceManager.hpp/.cpp`
  - 新增 `RHIFormatBytesPerTexel()`，ResourceManager 根据 Asset storage 映射 `RGBA8_*`、`RGBA16_Float`、`RGBA32_Float`。
  - 创建 GPU texture 前同时校验 Asset layout、RHI format bytes-per-texel、row/layer/total staging bytes；upload request 显式携带 `rowBytes/layerBytes`。
  - 增加 `TextureUploadStatus`，区分 invalid payload 与 GPU allocation/upload failure；日志记录 source、storage、RHI format、extent、layer/mip 和 staging bytes。
- `EnvironmentResources.hpp/.cpp`
  - `EnvironmentResourceStatus` 增加 texture decode、unsupported EXR、invalid payload 状态；GPU failure 继续独立报告。
- `tests/unit/TextureLoaderTests.cpp`、`EnvironmentResourceTests.cpp`、`tests/CMakeLists.txt`
  - 测试运行时生成确定性 Radiance HDR/OpenEXR fixture，不提交来源不明的大体积素材。
  - 覆盖大于 `1.0` 的动态范围、EXR RGB/alpha fallback、有限负值、NaN、unsupported EXR、Float16/Float32 byte layout、六面顺序/尺寸/混合编码、LDR 回归和环境错误状态。
- `Assets/Textures/Skybox/default-skybox.texture`
  - 把未实际生成 mip 的 `generateMips` 改为 `false`，避免 descriptor 声明与 mip 0-only payload 不一致。

### Architecture

```text
LDR source -----------------> stb_image UNorm8 --------------------+
Radiance .hdr -> stbi_loadf -> linear RGBA32 temporary -> F16/F32 |
OpenEXR ------> TinyEXR ----> linear RGBA32 temporary -> F16/F32 |
                                                                  v
TextureData(storage + rowBytes + layerBytes + sourceEncoding)
  -> AssetManager detailed load status
  -> ResourceManager validates storage/RHI byte agreement
  -> TextureDesc + tight staging payload + TextureUploadRequest
  -> TextureGPU(real RHI format)
  -> EnvironmentResourceResolver detailed status
```

Asset 层只描述 CPU 像素 storage，不保存 `VkFormat` 或依赖 RHI；Resource 层是 Asset storage 到 RHI format 的唯一映射点。HDR/EXR 的半精度转换发生在加载阶段，运行时每帧不做格式转换。Skybox Pass 仍统一消费 `samplerCube`，因此 6.1A 没有增加 Shader 格式分支。

### Verification

- `cmake --build build --target ChikaAsset ChikaEnvironmentResourceTests`：通过。
- `cmake --build build --target ChikaTextureLoaderTests ChikaEnvironmentResourceTests`：通过。
- `ctest --test-dir build -R "Chika\.(TextureLoader|EnvironmentResource)$" --output-on-failure`：2/2 通过。
- `cmake --build build`：通过；Editor、Game、Benchmark、Runtime libraries 与全部测试 target 均成功编译链接。
- `ctest --test-dir build --output-on-failure -R "Chika\.(TextureLoader|EnvironmentResource|AssetPipeline|RenderBaseline)$"`：4/4 通过。
- `ctest --test-dir build --output-on-failure -E "Chika\.(JobStress|JobSystem)$"`：17/17 通过。
- 非压力全集仅排除 `Chika.JobStress` 时在外层 120 秒超时且没有返回结果；进一步排除已有独立超时风险的 `Chika.JobSystem` 后其余测试全部通过。该 Jobs 问题与本次纹理链路无关，未在 6.1A 中掩盖或重构。

### Remaining Work

- Step 6.2A 已完成单张 2:1 equirectangular HDR/EXR 到 Cubemap 的确定性转换、项目配置与实际 Skybox sampling。
- 真实 float Cubemap 已通过格式/extent/staging 自动测试、Vulkan Validation runtime smoke 与截图验证；当前机器没有 `renderdoccmd`，`.rdc` capture 仅保留为可选人工复核。
- 普通 Cubemap mip 生成、diffuse irradiance、specular prefilter 与 BRDF LUT 仍属于后续卡片，不应在 Loader 中伪造。

---

## 2026-07-15 - TinyEXR Submodule 与构建目标接入

### Metadata

- Area: ThirdParty / CMake / Asset Foundation / Docs
- Related step: `docs/render/dev/steps/6.1a-hdr-exr-texture-contract.future.md`
- Status: Complete

### Goal

按仓库现有 ThirdParty 规划，把 TinyEXR 作为正式 Git submodule 放入 `engine/ThirdParty`，并接入统一 CMake 构建图，为后续 EXR float texture decoder 提供稳定依赖。本次不实现 EXR 业务解码。

### Changes

- `.gitmodules` / `engine/ThirdParty/tinyexr`
  - 新增官方 `https://github.com/syoyo/tinyexr.git` submodule。
  - 固定到 `v3.2.0`，gitlink commit 为 `6f470c9ab24bf3992bc512ce07e8ecb00d9bf105`。
  - 许可证为 BSD-3-Clause；依赖版本由主仓库 gitlink 固定，不跟随远端分支漂移。
- `engine/ThirdParty/CMakeLists.txt`
  - 不调用上游仅供 compile-test 使用的 `CMakeLists.txt`，避免它修改全局 C++ 标准、warning 和 sample 选项。
  - 新增 `tinyexr` 静态库和 `tinyexr::tinyexr` alias，编译官方 v3 C11 `src/*.c` 与内置 zstd adapter。
  - 使用 TinyEXR in-tree DEFLATE 路径，不额外增加系统 zlib/libdeflate 依赖；不构建 examples、tests、CLI、CUDA/Vulkan 可选 backend。
  - 将 `tinyexr::tinyexr` 加入 `ChikaThirdParty`，保持现有 Runtime target 的统一 ThirdParty 入口。
- `README.md` / `docs/cmake/ref/build-after-pull.md`
  - 补充 TinyEXR submodule、版本、license、target 与构建边界。
- `docs/render/dev/steps/6.1a-hdr-exr-texture-contract.future.md`
  - 标记 TinyEXR dependency foundation 已完成；EXR decoder、float payload 和 GPU upload 仍保持 planned。

### Architecture

```text
.gitmodules
  -> engine/ThirdParty/tinyexr @ v3.2.0
  -> tinyexr static C11 target
  -> tinyexr::tinyexr alias
  -> ChikaThirdParty interface
  -> ChikaAsset (future EXR decoder consumer)
```

依赖接入与 EXR 业务实现分离：ThirdParty target 只负责可重复编译和公开 `include/exr.h`；Asset 层后续负责 decoder API、错误转换和 TextureData 语义。

### Verification

- `cmake --build build --target tinyexr`：通过；编译 TinyEXR v3 C11 hosted CPU/stdio 源集与内置 zstd adapter，未构建 optional GPU backend，最终生成 `tinyexr.lib`。
- `cmake --build build`：通过；Editor、Game、Benchmark、Runtime libraries 和全部测试 target 均成功链接。
- `ctest --test-dir build -R "Chika\\.(AssetPipeline|EnvironmentResource|ProjectDescriptor)$" --output-on-failure`：3/3 通过。

### Remaining Work

- Step 6.1A 仍需实现 EXR decode、float payload、format-aware staging 与 GPU upload。
- 后续 Asset target 应显式链接实际使用的 dependency；当前先通过仓库统一 `ChikaThirdParty` 接入，保持现有依赖结构。
- 若未来启用 TinyEXR libdeflate、threads 或 GPU backend，必须新增独立性能/平台验证，不能隐式打开。

---

## 2026-07-15 - HDR/EXR Skybox 输入规划

### Metadata

- Area: Asset / Resource / RHI / Render / Docs
- Related steps: `docs/render/dev/steps/6.1a-hdr-exr-texture-contract.future.md`, `docs/render/dev/steps/6.2a-hdr-exr-skybox-import.future.md`
- Status: Superseded by completed Step 6.1A and Step 6.2A implementations

### Goal

纠正“`.hdr` 扩展名可读取等于 HDR 已支持”的误解，并规划从 Radiance HDR/OpenEXR 输入到浮点 Cubemap、RenderGraph 导入和 Skybox 采样的完整闭环。本次只修改文档，不修改代码、Shader、CMake、配置或素材。

### Current Finding

- 当前 PNG/JPEG/TGA 等 LDR 输入通过 `stbi_load` 解码为 RGBA8，可由现有资源链上传。
- `.hdr` 当前也走 `stbi_load(..., 4)`，高动态范围被量化为 8-bit；`.exr` 没有正式解码入口。
- `TextureData` 只有 `std::vector<uint8_t>`，ResourceManager 只在 `RGBA8_SRGB/RGBA8_UNorm` 中选择；RHI 虽已有 `RGBA16_Float/RGBA32_Float`，但尚未贯通纹理资产上传。
- 仓库中的 TinyEXR 只位于 `tinygltf/examples`，不能作为稳定的引擎依赖边界直接使用。

### Documentation Changes

- 新增 Step 6.1A，规划 float pixel storage、Radiance `stbi_loadf`、正式 TinyEXR target、format-aware staging/upload 和动态范围测试。
- 新增 Step 6.2A，规划 six-face HDR/EXR 与单张 2:1 equirectangular HDRI 两种输入、CPU import-time Cubemap conversion、项目配置、fallback 和四路径采样验收。
- 更新 Step 6.1，明确当前可靠能力是 LDR RGBA8，`.hdr` 仅能读取但不保真，`.exr` 未实现。
- 更新 Step 6.2，明确现有 Skybox Pass 格式无关，但当前视觉验收只覆盖 LDR Cubemap。
- 更新 Step 6.3，使 IBL 依赖已经验证的 float Cubemap，不重复承担 decoder/projection，并禁止把普通 mip 冒充 specular prefilter。

### Architecture Decision

```text
6.1A: HDR/EXR decoder
  -> backend-independent float TextureData
  -> format-aware Resource upload
  -> RGBA16_Float / RGBA32_Float

6.2A: .texture projection contract
  -> six faces or equirectangular source
  -> deterministic import-time Cubemap conversion
  -> Environment.Skybox
  -> existing format-independent Skybox Pass

6.3: float Cubemap
  -> irradiance / specular prefilter / BRDF LUT
```

Asset 层不保存 Vulkan format；Resource 层负责映射到 RHI format。HDR 解码和 projection 只在 asset load/reload 时执行，不进入逐帧渲染路径。Skybox 继续统一消费 TextureCube，不在 shader 中增加 equirectangular 特例。

### Verification

- `rg` 已确认 Step 6.1A -> 6.2A -> 6.3 的依赖、Next 链接和 HDR/EXR 能力边界表述一致。
- `git diff --check`：通过；仅报告工作区既有的 LF/CRLF 转换警告，没有 whitespace error。
- 本次没有运行 build/ctest，因为没有修改任何编译或运行文件。

### Remaining Work

- Step 6.1A 与 Step 6.2A 已按顺序实现，HDR-preserving Radiance/OpenEXR Skybox 能力已通过自动测试与 runtime smoke 验收。
- TinyEXR 的正式来源、版本、license 和 CMake target 已通过 submodule 落地。
- 已提交可重建的 CC0 程序化 HDR/EXR sample；单元测试继续使用小型确定性 fixture。
- Cubemap 普通 mip、IBL irradiance/specular prefilter 与 Cooker 缓存仍需后续卡片实现。

---

## 2026-07-15 - Skybox 配置加载与 LDR 色彩修复

### Metadata

- Area: Project / Editor / Game / Asset / Render / Test / Docs
- Related step: `docs/render/dev/steps/6.2-skybox-render-pass.future.md`
- Status: Complete

### Goal

修复运行程序只显示纯色 fallback、默认 Cubemap 颜色被洗成灰白的问题，并把天空盒从 Editor 入口硬编码迁移为 Editor/Game 共用的 Project Runtime 配置。

### Root Cause

- 诊断截图中纯蓝区域像素固定为 `99,147,175`，与 `fallbackColor={0.1,0.2,0.3}` 经 ACES/gamma 后的结果一致；它不是任何一张 Cubemap face。
- LLDB 在旧二进制的配置函数返回后确认 `EnvironmentSettings.enabled == false`，反汇编对应写入为 `movb $0x0`，导致 Resolver 保持 `Disabled`，资源没有进入上传和 Skybox Pass。工作区源码写的是 `true`，说明运行产物与源码发生漂移；仅靠入口局部硬编码无法形成可验证配置契约。
- 配置启用后 Cubemap 已能随方向采样，但默认六面来源是 LDR PNG，而 descriptor 使用 `srgb=false`。sRGB 字节被当成线性亮度后再次经过 ACES 和 gamma，使亮部范围压缩并呈现灰白。

### Changes

- `ProjectDescriptor.hpp/.cpp`
  - `ProjectRuntimeSettings` 增加完整 `EnvironmentSettings`。
  - 解析 `runtime.environment.enabled/skybox/intensity/useFallback/fallbackColor`，并验证对象类型、非负有限强度、四分量有限颜色及无 fallback 时必须存在资源引用。
  - `BuildRuntimeBootConfig()` 继续按值投影同一份配置，避免 Runtime 持有 JSON 或 Project 可变状态。
- `ChikaProject.json`
  - 显式启用默认 Skybox，并使用稳定 Texture GUID 与诊断路径。
  - 默认天空盒加入 `alwaysCook` 根集合；Cubemap 六面依赖的 Cook 展开仍由后续 Cooker 规则负责。
- `engine/Editor/src/main.cpp`
  - 删除默认 Cubemap 的局部硬编码，支持 `--project <path>`，默认读取 `ChikaProject.json`。
  - Editor 把 Project 中的环境配置应用到 Renderer；非法 Project 直接返回错误码 `2`。
- `engine/Game/src/main.cpp`
  - Game 在加载启动 Scene 前应用 `RuntimeBootConfig.runtime.environment`，与 Editor 共用解析和渲染链。
- `Assets/Textures/Skybox/default-skybox.texture`
  - LDR PNG Cubemap 改为 `srgb=true`，由 Vulkan sRGB image format 完成采样前线性化。
- 测试
  - `ProjectDescriptorTests` 覆盖环境配置解析、Development Runtime 投影和非法配置拒绝。
  - `EnvironmentResourceTests` 验证仓库默认 LDR Skybox 上传为 `RGBA8_SRGB`；独立线性环境 fixture 仍保留 `srgb=false` 语义测试。

### Architecture

```text
ChikaProject.json runtime.environment
  -> ProjectDescriptor validation
  -> ProjectRuntimeSettings
       -> EditorApplication
       -> RuntimeBootConfig -> GameApplication
  -> Renderer::SetEnvironmentSettings
  -> EnvironmentResourceResolver
  -> AssetManager -> ResourceManager upload/cache
  -> RenderGraph Environment.Skybox
  -> Skybox Pass -> HDR -> ACES/gamma
```

Project 只保存稳定引用和渲染策略；Asset/Resource/RHI 所有权边界不变。`EnvironmentSettings.enabled` 默认仍为 false，只有项目显式配置才启用环境资源。

### Verification

- `cmake --build build --target ChikaEditor ChikaGame ChikaProjectDescriptorTests ChikaEnvironmentResourceTests`：通过。
- `ctest --test-dir build -R "Chika\.(ProjectDescriptor|EnvironmentResource|RenderGraphSkyboxPass|ShaderInterface|RenderBaseline)$" --output-on-failure`：5/5 通过。
- LLDB 在修复后二进制首帧确认 `m_settings->environment.enabled == true`，GUID、expected type 和路径与 `ChikaProject.json` 一致。
- `ChikaGame.exe --smoke-frames 2`：退出码 `0`，记录 `Environment skybox resource is ready`。
- 隐藏运行 `ChikaEditor.exe` 的 Forward/Deferred × CPU/GPU-driven 四种组合：均正常退出并记录 `Environment skybox resource is ready`，未发现 `VUID`、`Validation Error` 或 `ERROR`。
- 实际截图验证：启用前天空区域固定为 fallback 的 `99,147,175`；启用并改用 sRGB 后，天空视角像素随位置和方向变化，不再是 fallback clear。
- `clang-format -i`：本次 C++ 文件已格式化；`git diff --check`：通过。

### Remaining Work

- 当前配置粒度是 Project Runtime；Scene 级环境覆盖、Inspector authoring、运行时切换与保存尚未实现。
- 默认基准相机俯视地面，启动视图主要看到 Cubemap 的低对比度上半天空；这不影响资源采样，但后续视觉回归应增加独立的高对比度测试 Cubemap 或固定天空视角。
- IBL、HDR 源格式、Cubemap mip 生成、rotation 控件仍属于后续环境光照步骤。

---

## 2026-07-15 - Skybox Render Pass 完整接入

### Metadata

- Area: RenderGraph / RenderPipeline / Shader / Editor / Test / Docs
- Related step: `docs/render/dev/steps/6.2-skybox-render-pass.future.md`
- Status: Complete

### Goal

完成 Skybox 从 `Environment.Skybox` 到 HDR Scene Color 的实际渲染闭环，并统一 CPU/GPU-driven、Forward/Deferred 四条路径的顺序、LoadOp、SceneDepth 和 fallback 行为。

### Changes

- `Assets/Shaders/skybox.vert` / `skybox.frag` 及导入产物
  - fullscreen triangle 由 `gl_VertexIndex` 生成，不创建 mesh/VBO。
  - 独立 `SkyboxData` UBO 保存 rotation-only inverse view-projection、intensity、Deferred depth 开关与统一 clear-depth 参数。
  - Fragment shader 输出线性 HDR；曝光和 tone mapping 仍只由 Post Process 负责。
- `RenderPipelinePasses.hpp/.cpp`
  - 增加 `AddSkybox()` 与 `AddGpuDrivenForward()`，调用方明确传入 HDR `LoadOp` 和是否采样 `SceneDepth`。
  - 增加 `MakeSceneDepthDescription()`，统一首次创建、RenderGraph import 和 resize 的 `DepthStencilAttachment | Sampled` 契约。
  - 增加 `MakeSkyboxInverseViewProjection()`，在 CPU 侧去掉 View translation。
- `RenderPipeline.hpp/.cpp`
  - 创建/释放 Skybox UBO、shader、pipeline 和 reflection binding；仅在 Cubemap、pipeline 与全部 binding 有效时创建 pass。
  - Forward：`Skybox(Clear HDR) -> Scene(Load HDR, Clear Depth)`。
  - Deferred：`GBuffer -> Deferred Lighting(Clear HDR) -> Skybox(Load HDR, Sample Depth) -> Transparent`。
  - CPU 与 GPU-driven 共享同一 fallback/LoadOp 决策；没有可绑定 Cubemap 时首个 scene writer 始终 clear `fallbackColor`。
- `EnvironmentResources.hpp/.cpp`
  - 增加默认 Cubemap fallback 的 `ReadyFallback` / `FallbackUnavailable` 状态。
  - 缺失或类型错误时按 `useFallback` 决定是否解析仓库内默认 Cubemap；fallback 也失效时不发布 Blackboard handle。
- `RenderGraph.hpp/.cpp`
  - DebugSnapshot 增加 texture read、color LoadOp 和 depth attachment，便于直接审计 Skybox depth/HDR 契约。
- `engine/Editor/src/main.cpp`
  - 当时由编辑器基准场景显式启用默认 Skybox；后续已由本日“Skybox 配置加载与 LDR 色彩修复”记录迁移到 Project Runtime 配置。
  - 增加 `--deferred` 与 `--gpu-driven` 参数，支持四条渲染路径 smoke。
- 测试
  - 新增 `RenderGraphSkyboxPassTests.cpp`，覆盖四路径顺序、Forward/Deferred LoadOp、depth sampled/no-depth-write、无 Skybox clear、resize descriptor 和相机平移不变性。
  - 扩展资源测试，覆盖默认 fallback、关闭 fallback、fallback 失效和无效 descriptor 拒绝。
  - 扩展 shader reflection 与 GPU 数据布局测试。

### Architecture

```text
EnvironmentSettings
  -> EnvironmentResourceResolver
  -> Blackboard[Environment.Skybox]
  -> PassModules::AddSkybox
       -> SkyboxData UBO (rotation-only inverse VP, intensity, depth policy)
       -> Cubemap reflection binding
       -> optional Blackboard[SceneDepth] sampled read
       -> Blackboard[HDRSceneColor] clear/load write
  -> Scene / Transparent
  -> Post Process exposure + tone mapping
  -> Overlay
```

关键边界：

- Asset/ResourceManager 继续拥有 Cubemap 和 RHI view；Skybox pass 只消费本帧 Blackboard handle。
- Skybox 没有 depth attachment。Forward 不读深度；Deferred 把 SceneDepth 从 `DepthWrite` 转为 `ShaderResource` 后只采样 clear/far-depth 像素。
- fallback 决策发生在建图前。资源不可用时不创建 Skybox pass，也不会在未初始化 HDR 上使用 `LoadOp::Load`。
- `SkyboxData` 与 `SceneData` 分离，防止环境显示参数污染场景/材质公共接口。

### Verification

- `glslc Assets/Shaders/skybox.vert ...` / `skybox.frag ...`：通过。
- 引擎 `ShaderReflection::Reflect()` 生成两份 SPIR-V sidecar；`Chika.ShaderInterface` 能解析 `skybox`、`EnvironmentSkybox`、`SceneDepth`。
- `clang-format -i <本次 C++ 文件>`：完成；`git diff --check`：通过。
- `cmake --build build`：通过。
- `ctest --test-dir build -R "Chika\.(RenderGraphSkyboxPass|EnvironmentResource|ShaderInterface|RenderBaseline|RenderPhase4)" --output-on-failure`：5/5 通过。
- `ctest --test-dir build -E "Chika\.(JobSystem|JobStress)" --output-on-failure`：16/16 通过。
- `ctest --test-dir build -R "Chika\.JobStress$" --output-on-failure`：1/1 通过。
- 编辑器 Vulkan smoke：Forward CPU、Deferred CPU、Forward GPU-driven、Deferred GPU-driven 均记录 `Environment skybox resource is ready`，未出现 `VUID`、`Validation Error`；测试窗口在 6-8 秒后主动终止，因此进程 `ExitCode=-1` 是 smoke harness 的预期结果。
- 全量 `ctest --test-dir build --output-on-failure` 未完成：与本次渲染改动无依赖的 `Chika.JobSystem` 单测在当前机器连续等待超过 90 秒，导致总任务超时；其余 17 项均已分别通过。

### Remaining Work

- 本步骤不包含 IBL、Cubemap HDR 格式升级、mip 生成、动态天空或 rotation 控件；这些继续由 6.3 及后续卡片承担。
- 尚未保存 RenderDoc 帧捕获；RenderGraph snapshot、自动测试和 Vulkan validation 已覆盖本次代码验收。
- `Chika.JobSystem` 的独立超时需要在 Jobs 模块任务中排查，不应在 Skybox 改动中掩盖或重构。

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
