# Renderer Step 2.4：不可变 RenderWorld Snapshot

## Metadata

- Date: 2026-06-14
- Status: Complete
- Main files: `RenderWorld.hpp/.cpp`, `Renderer.hpp/.cpp`, `RenderPipeline.cpp`

## Goal

保证 Renderer 帧内只消费稳定只读输入，不读取正在变化的 RenderWorld 或 Gameplay。

## Implementation

- `RenderWorld::CreateSnapshot()` 在帧边界复制 Object、Light 和 View 数据。
- Snapshot 使用 `shared_ptr<const RenderWorldSnapshot>` 提交。
- Snapshot 保存唯一 `worldId` 与 Revision，避免不同 Scene 的 Proxy Handle 冲突。
- Renderer Facade 转交 Snapshot；RenderPipeline 是唯一消费方。
- 旧 Snapshot 在 RenderWorld 更新或删除后保持不变。

## Verification

- 单元测试验证 Snapshot 与后续 World Mutation 隔离。
- RenderPipeline 源码不依赖 Framework 模块。

## Remaining Boundary

当前 Snapshot 复制完整 Proxy 数据；未来 Render Thread 可改为双缓冲、结构化数组或命令增量应用。
