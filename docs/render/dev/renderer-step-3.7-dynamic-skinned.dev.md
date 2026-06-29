# Renderer Step 3.7：动态与蒙皮对象路径

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderSubsystem.cpp`, `RenderPipeline.cpp`, `ResourceManager.cpp`, `skinned.vert`

## Goal

将蒙皮 GPU 数据所有权留在 Renderer，并明确区分静态实例对象与动态对象。

## Implementation

- Gameplay `Animator` 只生成骨骼矩阵；RenderSubsystem 将矩阵复制到 Render Proxy，Gameplay 不持有 RHI Handle。
- RenderPipeline 按 `worldId + RenderObjectHandle` 管理每个蒙皮对象的可增长 Storage Buffer。
- Buffer 大小按实际骨骼矩阵数量分配，Shader 使用 unsized Storage Buffer，删除固定 `128` 骨骼 UBO。
- 蒙皮对象不参与静态实例 Batch，使用对象 Transform Push Constant 和独立骨骼 Buffer。
- 对象离开 Snapshot 后，Buffer 通过 RHI 延迟销毁队列回收，避免 GPU 使用期间释放。

## Design Reason

该所有权方向接近 Unreal 的 Gameplay Animation State 与 Render-side Skinning Data 分离：组件描述动画结果，Renderer 决定 GPU 内存布局、更新时机和生命周期。

## Verification

- Play Mode Fox 生成 `1536` 字节 Storage Buffer，即 24 个实际骨骼矩阵。
- 静态搜索确认 `UploadBoneMatrices`、固定 `boneMatrices[128]` 和固定 128 矩阵分配已移除。
- Forward Play Mode 与 Deferred Edit Mode 均无 Vulkan Validation Error。

## Remaining Boundary

当前每个蒙皮对象拥有独立 Buffer；后续应迁移到统一动态 Buffer Arena，并增加动画 Bounds、GPU Skinning/Compute Skinning 和骨骼上传去重。
