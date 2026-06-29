# Renderer Step 4.3：Compute、Copy 与 Depth-only Pass

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderGraphPass.hpp`, `RenderGraph.cpp`, `RenderPipeline.cpp`, `ResourceManager.cpp`

## Goal

让非 Graphics 工作成为正式 Pass，并删除 Shadow Dummy Color 绕行。

## Implementation

- `RGPassType` 明确 Graphics、Compute、Copy、Present。
- `RGPass` 分离普通 `textureWrites` 与 Graphics `colorWrites`；`WriteTexture` 只声明 Copy/Storage 等普通写入，`WriteColor` 才声明 Color Attachment。
- Upload 使用 Copy Pass 声明源/目标资源，Texture 与 Buffer 状态由 Graph 转换。
- Shadow Pass 仅声明 Depth Attachment。
- 每个材质新增 Vertex-only Shadow Pipeline，避免用带 Fragment Color 输出的 Forward Pipeline 执行 Depth-only Rendering。
- RHI 增加 Compute Pipeline、Dispatch 和 Indirect Draw。

## Design Reason

Pass 类型决定合法资源声明和命令录制边界。Depth-only 不应创建无意义 Color Texture，上传也不应在 Graph 外手写 Barrier。

## Regression Fix

最初实现把 `WriteTexture` 也写入 `colorWrites`，导致 Upload Copy Pass 被 Compile 误判为“非 Graphics Pass 声明 Attachment”。首帧 Graph 因此停止执行，而已经取出的上传任务不会在后续帧重试，最终表现为 Fox/Cube 顶点、索引和纹理没有上传，只剩底色与编辑器碰撞箱。

修复后普通 Texture 写入与 Attachment 写入拥有独立声明集合；Vulkan Present 同时保证消费空命令帧仍会 signal 的 `renderFinished` binary semaphore，避免首帧失败继续污染后续帧。

## Verification

测试确认 Depth-only Pass 的 Color Attachment 数为 0，Compute Pass 执行一次 Dispatch，Copy Pass 写入 Texture 不会开始 Graphics Rendering。Forward/Deferred 实际运行均无 RenderGraph/Vulkan Validation 错误，窗口截图确认 Fox、地面与 Cube 恢复绘制。
