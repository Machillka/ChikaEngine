# Direct Light Shadow 与 Spot Light 最小修复记录

## Metadata

- Status: Implemented in this step
- Area: Renderer / Framework LightComponent / Shader
- Scope: Single directional shadow caster, direct lighting, spot light data path

## Problem

当前直接光路径仍有两个明显问题：

- Shadow pass 已存在，但 Renderer 使用 `RenderWorldSnapshot::lights.front()` 作为 shadow view 和 `lightVP` 来源；当第一盏灯不是投射阴影的方向光时，shader 第 0 盏灯无法正确读取 shadow。
- Forward 与 Deferred 的方向光方向和环境兜底不完全一致，Forward 方向光会使用错误方向，导致表面明暗看起来不稳定。
- Shadow 采样是硬二值比较，PCF 边缘仍容易出现明显锯齿。
- `LightComponent` 只能生成 Directional Light，Editor 无法配置 Point/Spot 所需的 type、range 和 cone 参数。

## Minimal Implementation

本次只做最小闭环，不扩展完整阴影系统：

1. Renderer 选择第一盏 `castsShadow == true` 且 `type == Directional` 的光作为本帧唯一 shadow caster。
2. `SceneData.lightVP`、shadow queue 的 view，以及 GPU light buffer 的第 0 项使用同一盏 shadow caster。
3. 如果没有有效 directional shadow caster，shadow queue 使用空 layer mask，避免把主相机深度误写成 shadow map。
4. Forward/Deferred shader 使用一致的方向光方向、spot cone attenuation 和 ambient fallback。
5. Shadow PCF 使用实际 `textureSize(shadowMap, 0)` 作为 texel size，并对 depth compare 做很窄的平滑过渡，减少硬边锯齿。
6. `LightComponent` 增加可反射字段：
   - `lightType`: `0=Directional`, `1=Point`, `2=Spot`
   - `range`
   - `innerConeDegrees`
   - `outerConeDegrees`
7. Spot light 只实现直接光照，不在本步骤实现 spot shadow 或 shadow atlas。

## Changed Files

- `engine/Runtime/Render/src/RenderPipeline.cpp`
- `Assets/Shaders/test.frag`
- `Assets/Shaders/deferred_lighting.frag`
- `engine/Runtime/Framework/include/ChikaEngine/component/LightComponent.hpp`
- `engine/Runtime/Framework/src/component/LightComponent.cpp`

## Verification

- 编译 forward/deferred fragment shaders。
- 构建 `build`。
- 运行 render/framework/editor 相关 smoke tests。

## Boundaries

- 不新增 CSM。
- 不新增 shadow atlas。
- 不新增 point/spot shadow。
- 不改 RHI pipeline 架构或 RenderGraph pass 拆分。
- 不把光源编辑做成独立资产系统；当前仍是 Component 字段。
