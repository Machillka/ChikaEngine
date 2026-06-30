# Renderer Step 4.1：RenderGraph Buffer Resource

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderGraph.hpp`, `RenderGraphResource.hpp`, `RenderPassBuilder.hpp`, `RenderGraph.cpp`

## Goal

让 Buffer 与 Texture 一样进入 RenderGraph 的声明、依赖、生命周期和 transient 复用。

## Implementation

- 增加 `RGBufferHandle`、`RGBufferResource`、`RGBufferUsage`。
- Builder 增加 Create/Read/Write Buffer；Graph 增加 Import/Get Physical Buffer。
- Copy、Compute、Graphics 之间按 Buffer RAW/WAR/WAW 依赖排序。
- transient Buffer 按完整 `BufferDesc` Hash 复用，并在最后使用后归还池。

## Design Reason

RenderGraph 管理的是 GPU 工作依赖，而不是只管理 Attachment。Instance、Storage、Indirect 和 Compute 输出若绕过 Graph，Barrier 和生命周期就无法统一。

## Verification

`Chika.RenderBaseline` 覆盖 Copy 写 Buffer、Compute 读写 Buffer、Graphics 读取结果的完整链路。
