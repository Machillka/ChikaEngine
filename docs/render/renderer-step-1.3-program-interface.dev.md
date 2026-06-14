# Renderer Step 1.3：Shader Program Interface

## Metadata

- Date: 2026-06-13
- Status: Complete
- Main files: `ShaderInterface.cpp`, `ResourceManager.cpp`, `renderer.cpp`

## Goal

在创建 Pipeline 前合并 Stage 接口并报告冲突，避免把错误推迟到 Vulkan Validation。

## Implementation

- `BuildShaderProgramInterface()` 合并相同 set/binding 的 Stage 可见性。
- 校验 Descriptor 名称、类型、数组长度、Buffer Layout 和 Push Constant Layout。
- 校验 Fragment Input 是否存在类型一致的 Vertex Output。
- Vulkan Pipeline 创建时校验 Fragment Output 数量、location 和 Attachment 类型。

## Verification

- 单元测试覆盖兼容合并、Descriptor 冲突和 Stage 接口类型冲突。
- 当前 Forward、Skinned Forward、GBuffer、Deferred Program Interface 全部通过。

## Design Result

Shader Stage 不再被独立、盲目地拼接；Pipeline 创建前已有可读的接口错误。
