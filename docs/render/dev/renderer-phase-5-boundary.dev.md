# Renderer Phase 5：成熟渲染功能边界

## Metadata

- Date: 2026-06-15
- Status: In Progress
- Depends on: Phase 0-4

## Goal

在不绕过 RenderWorld、RenderGraph、Shader Reflection 和 RHI 的前提下，建立可持续扩展的画面功能基础。

## 本阶段实现边界

- Scene Color 与显示输出分离：场景在线性 `RGBA16F` 中渲染，最终 Post Process 输出 LDR。
- Metallic-Roughness PBR：Forward 与 Deferred 共用相同 BRDF 和材质参数。
- 多光源：RenderWorld Light Proxy 上传为帧级 Storage Buffer，Shader 支持方向光、点光和聚光灯。
- 透明：沿用独立远到近队列，使用 Alpha Blend、关闭 Depth Write，并在 Tone Mapping 前合成。
- 阴影：保留单方向光 Shadow View，升级为可配置 Bias 与 PCF。
- 后处理：建立独立 Tone Mapping Pass，并提供 Exposure、Bloom 与 FXAA 配置入口。
- 回归体系：增加 Shader Interface、Pass 顺序、HDR Format 与设置默认值测试。

## 明确不伪装完成的高级功能

- IBL 当前只建立环境光强度入口；Cubemap 导入、Irradiance、Prefilter 与 BRDF LUT 仍需正式资产与 RHI Cube View 支持。
- 多光源当前使用正确的全量 Light Buffer 循环；Tiled/Clustered Light Culling 需要 Compute Light List，不能用 CPU 占位冒充。
- 阴影当前为单方向光 Shadow Map；CSM、Shadow Atlas 和局部光阴影属于后续增量。
- Bloom 当前作为 Post Process Composite 中的轻量邻域高光扩散；正式多级降采样/上采样 Bloom 仍需独立 Pass 链。

## 数据流

```text
RenderWorldSnapshot.lights
    -> RenderPipeline Light Storage Buffer
    -> Forward / Deferred PBR

Opaque / Deferred Lighting / Transparent
    -> HDR Scene Color
    -> Post Process Composite
    -> LDR Scene Color
    -> Editor Viewport / UI
```

## 设计约束

- Shader 资源布局继续由 Reflection 驱动，不新增固定 binding C++ 业务逻辑。
- Material 参数 offset 继续由 Reflection 写入。
- 后处理通过 Blackboard 语义连接，不由 Renderer 主类手工传递 RG Handle。
- Gameplay 与 Framework 不持有 Light Buffer、Post Process Pipeline 或其他 RHI Handle。
