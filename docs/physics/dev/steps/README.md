# Physics Layer Step Cards

本目录是物理层调整的实施顺序与状态源。总体架构、当前缺口和里程碑见 `docs/physics/plan/physics-layer-roadmap.md`。

## Status convention

- `Planned`：仅完成规划，尚未修改代码。
- `In Progress`：实现和验证尚未闭环。
- `Implemented`：卡片验收标准与文档同步完成。
- `Blocked`：存在明确外部阻塞，卡片内必须记录原因。

## Ordered cards

| Order | Card | Priority | Status |
| --- | --- | --- | --- |
| 1 | [Step 0.1 - Contract and Runtime Ownership](0.1-physics-contract-runtime.future.md) | Core | Implemented |
| 2 | [Step 0.2 - Body Lifecycle and Command Buffer](0.2-body-lifecycle-command-buffer.future.md) | Core | Implemented |
| 3 | [Step 1.1 - Contact State Stream](1.1-contact-state-stream.future.md) | Core | Implemented |
| 4 | [Step 1.2 - Collision and Trigger Broadcast](1.2-collision-trigger-broadcast.future.md) | Core | Implemented |
| 5 | [Step 2.1 - Collider and Rigidbody Authoring](2.1-collider-rigidbody-authoring.future.md) | Core | Implemented |
| 6 | [Step 2.2 - Transform and Motion Contract](2.2-transform-motion-contract.future.md) | Core | Planned |
| 7 | [Step 3.1 - Collision Layers and Profiles](3.1-collision-layers-profiles.future.md) | Core | Planned |
| 8 | [Step 3.2 - Scene Query Suite](3.2-scene-query-suite.future.md) | Core | Planned |
| 9 | [Step 4.1 - Shapes, Materials, CCD and Sleep](4.1-shapes-materials-ccd.future.md) | Extension | Planned |
| 10 | [Step 4.2 - Physics Constraints](4.2-physics-constraints.future.md) | Extension | Planned |
| 11 | [Step 5.1 - Character Controller](5.1-character-controller.future.md) | Extension | Planned |
| 12 | [Step 6.1 - Debug, Profiling and Regression Gates](6.1-physics-quality-gates.future.md) | Required gate | Planned |

## Execution rule

- 不跳过 0.1、0.2 直接实现消息广播，因为事件依赖稳定 Handle、Body registry 和销毁语义。
- 不在 2.1 之前继续把形状字段堆入 `Rigidbody`。
- 不在 3.1 之前扩展大量查询重载，因为 query filter 必须先与 layer/profile 语义统一。
- 每完成一张卡片，同步更新该卡片状态、路线图实际状态和 `docs/develop.md`。
- 每张卡片的自动化测试随实现一起交付，不推迟到 6.1；6.1 负责系统级汇总门禁。
