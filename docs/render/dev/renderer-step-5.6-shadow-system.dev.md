# Renderer Step 5.6：阴影系统基础

## Metadata

- Date: 2026-06-15
- Status: Single Directional Foundation Complete; CSM Pending

## Implementation

- Shadow 配置集中到 `ShadowSettings`：Resolution、Depth Bias、Normal Bias、PCF Radius。
- Forward/Deferred 使用一致的可配置 PCF 阴影采样。
- Shadow View 与 Shadow Depth 继续通过 RenderWorld、独立 Depth-only Pass 和 RenderGraph 管理。
- Light Buffer 标记光源是否投射阴影。

## Why

先把固定常量升级为稳定 Shadow API，再引入 CSM/Atlas；否则高级阴影仍会把业务状态写回 Renderer 主类。

## Next

Directional CSM、Shadow Atlas、稳定级联、局部光阴影与质量档位。
