# Renderer Step 5.3：IBL 边界

## Metadata

- Date: 2026-06-15
- Status: Boundary Complete; Asset Pipeline Pending

## Implementation

- `RenderSettings::ambientIntensity` 成为统一环境光强度入口。
- Forward/Deferred PBR 使用 Roughness、Metallic、F0 和 Occlusion 计算一致的环境响应。
- Editor Render Quality 面板可以实时调整 Ambient Intensity。

## Why

当前 RHI 尚无正式 Cubemap Texture/View 与环境预计算资产类型。使用明确的环境入口保留正确边界，但不把常量环境光冒充完整 IBL。

## Next

增加 Cubemap Importer、Irradiance Map、Prefiltered Environment Map、BRDF LUT 与 Reflection Binding。
