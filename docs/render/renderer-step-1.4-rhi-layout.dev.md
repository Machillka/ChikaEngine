# Renderer Step 1.4：RHI Pipeline Layout 与资源绑定

## Metadata

- Date: 2026-06-13
- Status: Complete
- Main files: `RHIDesc.hpp`, `ResourceBinder.hpp/.cpp`, `IRHICommandList.hpp`

## Goal

让 RHI 明确表达 Pipeline Layout 和 Descriptor 类型，不再根据 Buffer/Texture 猜测绑定含义。

## Implementation

- `PipelineDesc` 接收合并后的 `ShaderProgramInterface`。
- `ResourceBindingGroup` 明确保存 set、binding、Descriptor Type、数组元素和生命周期。
- 支持 Uniform/Storage Buffer、Combined/Sampled/Storage Image 与独立 Sampler 描述。
- Pipeline/Material 创建阶段通过资源名称解析 `ResourceBindingHandle`；运行帧按稳定 set/binding/type/array element 绑定。
- `BindResources()` 不再额外接收手写 set，Descriptor Array 越界会在提交前被拒绝。
- Push Constant 按反射范围名称提交。

## Verification

- 搜索 Renderer/Resource 不再存在 `BindBuffer(number)`、`BindTexture(number)` 或 `BindResources(0, ...)`。
- Mock RHI 与 Vulkan RHI 均使用新接口完成构建和测试。

## Design Result

RHI 接口可以表达多个 Descriptor Set 和未来 Compute/PBR 所需资源类型。
