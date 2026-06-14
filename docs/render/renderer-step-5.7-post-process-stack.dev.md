# Renderer Step 5.7：后处理输出链

## Metadata

- Date: 2026-06-15
- Status: Foundation Complete

## Implementation

- 新增独立 `Post Process Composite` Pass：读取 HDR Scene Color，写入 LDR Scene Color。
- 新增 Reflection 驱动的 `PostProcessData` UBO。
- 支持 Exposure、ACES Tone Mapping、Gamma、轻量 Bloom 与 FXAA 开关。
- Editor Render Quality 面板提供 Exposure、Ambient、Bloom、FXAA 实时调节。

## Why

统一输出链确保材质 Shader 只负责线性光照，后续效果可以通过 Blackboard 和独立 Pass 组合。

## Next

将 Bloom 拆为多级 Downsample/Upsample Pass；增加 TAA、SSAO、Motion Vector 和 History Resource。
