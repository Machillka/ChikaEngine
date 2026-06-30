# Renderer Step 1.8：删除旧硬编码绑定路径

## Metadata

- Date: 2026-06-13
- Status: Complete
- Main files: Renderer、ResourceManager、Vulkan RHI、Assets/Shaders

## Goal

保证 Shader、Material、Pipeline 和 Descriptor 只有一条 Reflection Binding 路径。

## Implementation

- 采用导入期校验的频率约定：
  - set 0：frame/scene
  - set 1：material
  - set 2：object
- Renderer 按 `scene`、`shadowMap`、`uboBones`、`GBuffer*` 名称绑定。
- Pipeline/Material 创建阶段将名称解析为 `ResourceBindingHandle`；逐帧与逐 Draw 路径只使用稳定的 set/binding/type。
- 删除固定 `0-3 UBO + 4-15 Texture` Layout、固定 128 字节 Push Constant 和手工 Material slot。
- Shader Importer 拒绝违反当前频率约定或使用 set 3+ 的 Shader。

## Verification

- `cmake --build build`：通过。
- `ctest --test-dir build --output-on-failure`：5/5 通过。
- Forward 与 Deferred Editor 运行：退出码 0。
- 日志无 `Validation Error`、`VUID`、接口冲突或 Vertex Attribute 警告。
- 搜索旧全局 Layout、数字业务 binding 和 Material `slot`：无结果。
- 搜索 Renderer 热路径中的 Shader Interface 名称查询：无结果。

## Next Step

Phase 2 可在稳定 Shader/Pipeline 契约上拆分 Renderer 并建立 RenderWorld。
