# Renderer Step 5.4：多光源数据路径

## Metadata

- Date: 2026-06-15
- Status: Correctness Path Complete; Clustered Pending

## Implementation

- `RenderWorldSnapshot::lights` 每帧转换为最多 128 项的 `LightGPU` Storage Buffer。
- 支持 Directional、Point、Spot；Spot Proxy 保存 inner/outer cone。
- Forward 与 Deferred Shader 正确遍历全部有效光源。
- Light Buffer 通过 Reflection 解析并绑定，不在 Renderer 中保存业务 binding 数字。

## Why

先建立正确且统一的多光源数据契约，才能让后续 Compute Pass 只替换可见 Light List，而不重写材质和 Scene Proxy。

## Next

实现 Tiled/Clustered Compute Light Culling 与 Tile/Cluster Light List。
