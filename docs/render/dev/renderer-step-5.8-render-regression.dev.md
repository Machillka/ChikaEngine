# Renderer Step 5.8：渲染质量与性能回归

## Metadata

- Date: 2026-06-15
- Status: Foundation Complete

## Automated Verification

- Shader Interface：PBR Forward/Skinned/GBuffer/Deferred/PostProcess 接口与输出数量。
- GPU Data Contract：`SceneData`、`LightGPU`、`PostProcessData` 的 std140/std430 大小。
- RenderGraph：HDR Scene -> Post Process -> LDR Present 顺序。
- Color Space：sRGB 与 Linear Format 分离。
- Existing：Visibility、Sort、Batch、Instancing、Barrier 与 Copy/Compute 路径。

## Runtime Verification

- Forward 与 Deferred 均连续运行，无 RenderGraph Compile Error、VUID 或 Validation Error。
- 固定场景截图确认 Fox、地面、Cube、阴影与 Tone Mapping 输出存在。

## Next

增加像素级截图阈值比较、正式 PBR 材质球场景、不同光源数量 GPU 时间预算与资源泄漏自动检查。
