# Renderer Step 5.1：HDR 与颜色空间基础

## Metadata

- Date: 2026-06-15
- Status: Foundation Complete

## Implementation

- 新增 `RGBA8_SRGB`，颜色纹理默认使用 sRGB 采样；GBuffer、HDR 与数据纹理保持 Linear。
- 场景几何写入 `RGBA16_Float` 的 `HDRSceneColor`，编辑器 Viewport 使用独立 `RGBA8_UNorm` LDR 输出。
- Blackboard 明确区分 `SceneColor.HDR` 与最终 `SceneColor`。

## Why

PBR 必须在线性 HDR 空间累积光照；显示转换属于输出阶段，不能写进每个材质 Shader。

## Verification

- 单元测试检查 sRGB/Linear Format 分离。
- Forward/Deferred 实际运行无 Vulkan Validation 错误。

## Pending

Texture meta 仍需增加可覆盖的 color-space import setting，供 Normal/ORM 等线性贴图使用。
