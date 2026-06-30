# Renderer Step 5.2：Metallic-Roughness PBR

## Metadata

- Date: 2026-06-15
- Status: Foundation Complete

## Implementation

- Forward 与 Deferred Shader 使用统一 GGX、Smith 与 Schlick Fresnel BRDF。
- Material Reflection Layout 增加 `BaseColor`、`Emissive`、`Metallic`、`Roughness`、`OcclusionStrength`、`NormalScale`。
- GBuffer 保存 Albedo/Metallic、Normal/Roughness、Emissive/Occlusion、WorldPosition/Alpha。
- Material Pipeline 的颜色目标升级为 `RGBA16_Float`。

## Why

先统一材质和 BRDF，再扩展 IBL、局部光与后处理，避免 Forward/Deferred 产生两套不一致的材质语义。

## Verification

- Shader Interface 测试确认 Forward、Skinned、GBuffer 与 Deferred 接口均可合并。
- GBuffer Reflection 确认包含四个输出。

## Pending

Normal、ORM、Emissive 贴图需要结合 Texture Import color-space 与 fallback texture 系统继续接入。
