# Renderer Phase 5 Feature Matrix

| Feature | Current Status | Verification | Next Increment |
| --- | --- | --- | --- |
| Linear HDR Scene Color | Implemented | Forward/Deferred runtime | HDR swapchain option |
| sRGB/Linear formats | Base implemented | Unit test | Texture meta import setting |
| Metallic-Roughness PBR | Base implemented | Reflection/runtime | Normal/ORM texture workflow |
| Environment lighting | Parameter boundary | Runtime | Cubemap/IBL precompute |
| Directional/Point/Spot lights | Correct full-loop path | GPU Light Buffer | Clustered culling |
| Transparent queue | Base implemented | Sort/runtime | Refraction/OIT |
| Directional shadow | PCF base implemented | Runtime | CSM/Atlas |
| Post process | Tone map/Gamma/Bloom/FXAA base | Graph/runtime | Multi-pass Bloom/TAA/SSAO |
| Quality regression | Structural/runtime base | 5 test targets | Pixel comparison/perf budgets |
