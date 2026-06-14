# Renderer Step 5.5：透明渲染路径

## Metadata

- Date: 2026-06-15
- Status: Foundation Complete

## Implementation

- Transparent Material 使用 Alpha Blend、关闭 Depth Write。
- Transparent Packet 保持按 View Depth 从远到近排序且不参与实例 Batch 合并。
- Deferred 中透明对象在 Lighting 后独立绘制；Forward 中在 Opaque 后绘制。
- 透明合成在线性 HDR Scene Color 中完成，随后统一 Tone Mapping。

## Verification

- Render Queue 单元测试覆盖反向深度排序和独立 Batch。
- Forward/Deferred 实际运行无 Attachment 或 Blend Validation 错误。

## Pending

折射、排序组、加权 OIT 与透明阴影未实现。
