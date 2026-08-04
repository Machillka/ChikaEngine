# Environment Assets

## Default Skybox

- Source asset: `Assets/Textures/Skybox/NightSkyHDRI008_2K_HDR.exr`
- Descriptor: `Assets/Textures/Skybox/NightSkyHDRI008_2K_HDR.texture`
- Source resolution: 2048x1024 equirectangular OpenEXR
- Runtime output: 512x512x6 `RGBA16_Float` Cubemap, 12582912 bytes
- Source: [ambientCG - Night Sky HDRI 008](https://ambientcg.com/a/NightSkyHDRI008)
- License: Creative Commons CC0

`ChikaProject.json` 通过稳定 AssetReference 引用 descriptor，而不是直接引用裸 EXR。descriptor 负责 projection、输出尺寸、浮点格式和 mip contract。

## Loading Behavior

EXR decode 和 equirectangular projection 由 Asset JobSystem 在工作线程执行。首帧立即返回 `EnvironmentResourceStatus::Loading` 并使用 `fallbackColor`；只有 CPU TextureCube 完成后，主线程才进行 RHI 创建和 staging upload。

2026-07-15 Debug runtime 基线：2048x1024 EXR 转换到 512x512x6 RGBA16F 约 938 ms。该成本不再阻塞窗口和首帧，后续 Cooker 缓存可继续降低后台 CPU/内存开销。

先前的 `procedural-studio.hdr/.exr`、对应 descriptor、`ChikaProject.exr.json` 与 `ChikaEnvironmentSampleGenerator` 仅用于 Step 6.2A 初始验收，现已删除。方向、seam 和 HDR 值仍由运行时生成的小型单元测试 fixture 覆盖。
