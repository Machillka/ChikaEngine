# Physics Layer Adjustment Roadmap

## Metadata

- Status: In Progress（M0 Stabilize Complete）
- Planning date: 2026-07-16
- Scope: Runtime Physics / Framework / Editor / Test / Docs
- Backend baseline: Jolt Physics
- Source of truth: `docs/physics/dev/steps/README.md`

## Goal

把当前“能够创建 Jolt Body 并执行基础模拟”的物理代码，调整为边界明确、事件完整、可配置、可测试的引擎物理层。核心闭环是：

```text
authoring components
  -> PhysicsSubsystem commands
  -> backend-neutral PhysicsScene
  -> Jolt simulation
  -> transform synchronization
  -> collision/trigger normalization
  -> main-thread scene broadcast and component callbacks
```

本路线优先保证刚体、碰撞、Trigger、查询和固定步长可靠，再扩展约束、角色控制器和高级形状。

## Current Baseline

### Already available

- `PhysicsCommandBuffer` 已统一 Body 结构与运动命令，并在 `PreStep` 按固定 phase 提交；同 owner 结构命令使用 last-intent coalescing。
- `PhysicsBodyRegistry` 已统一 engine handle、opaque backend token、owner 和 active state；同 owner 单 Body、atomic rebuild、stale handle 和清理统计已有测试锁定。
- Rigidbody runtime 生命周期已迁移为 deferred command；StartPlay/StopPlay、disable、destroy 和 Cleanup 不再遗留 pending command 或 Jolt Body。
- `PhysicsRuntime` 已通过引用计数 Lease 管理进程级 Jolt 注册，支持两个 PhysicsScene 同时存在并按最后一个 lease 释放。
- 公共 Body/Collider Handle 已使用 index + generation；Jolt BodyID 仅存在于 backend registry，stale 与 wrong-scene handle 会被拒绝。
- Physics 初始化、Body 创建和 capability 已形成可诊断契约；默认单位为米、秒、千克，Y-up、右手系，默认重力 `{0, -9.81, 0}`。
- `PhysicsScene` 按 Framework Scene 持有后端实例。
- Jolt 后端支持 Static、Kinematic、Dynamic Body。
- Scene 已使用可配置 `fixedDeltaTime` 和 `maxPhysicsStepsPerFrame` 驱动物理固定步长。
- 已有 Box、Sphere 创建路径，以及速度、冲量、Transform 回写和 closest Raycast。
- 后端已接入 Jolt `ContactListener`，并使用互斥队列收集 `OnContactAdded`。
- 已存在 32-bit layer mask 基础设施与 Scene `EventBus`。

### Gaps to close

1. `PollCollisionEvents()` 没有被 `PhysicsScene` 或 `PhysicsSubsystem` 消费，碰撞不会到达 GameObject、Component 或 Script。
2. `OnContactPersisted`、`OnContactRemoved` 为空，无法表达 Enter/Stay/Exit；Trigger 也没有独立语义。
3. Jolt 回调发生在物理工作线程且 Body 被锁定，当前还缺少“只采集、模拟后主线程归一化”的正式边界。
4. 原始事件只保存 Body Handle，缺少稳定 Collider ID、GameObject ID、事件阶段、Trigger 类型和销毁时语义。
5. `Rigidbody` 同时保存碰撞形状和动力学属性；没有独立 Collider，也无法自然表达静态碰撞体。
6. Capsule 枚举存在但后端未创建 Capsule；center、scale、`_colliderOffset` 和每 Body `collisionMask` 没有形成完整行为。
7. Runtime RAII、generation-safe Handle、Body registry 与 fixed-step command buffer 已由 Step 0.1/0.2 完成；下一阶段需要把 Body 销毁与 contact pair cleanup/Exit 语义接通。
8. Dynamic、Kinematic、Static 的 Transform 权威方向不完整；没有插值、Kinematic target 和正式 Teleport 语义。
9. 查询只有 closest Raycast，没有 query filter、multi-hit、overlap、shape cast、ignore self 或 Trigger 策略。
10. 缺少物理集成测试、事件顺序测试、层矩阵测试、资源泄漏门禁和可视化调试。

## Architecture Decisions

### Ownership

- `PhysicsRuntime`：进程级 RAII，负责 Jolt allocator、factory、type registration 等全局生命周期。
- `PhysicsScene`：每个 Framework Scene 一个物理世界，拥有后端、Engine Handle Registry、命令缓冲和 contact state。
- `IPhysicsBackend`：只表达后端能力，不向 Framework 暴露 `JPH::*` 类型。
- `PhysicsSubsystem`：负责 GameObject/Component 集成、固定步阶段编排、Transform 同步和事件发布。
- `Collider`：描述形状、材质、Trigger、layer/profile 和 query participation。
- `Rigidbody`：描述质量、MotionType、阻尼、重力、CCD、轴约束和运动命令；没有 Rigidbody 的 Collider 形成 Static Body。

### Fixed-step order

每个 fixed step 固定为：

1. `GameObject::FixedTick(fixedDeltaTime)` 生成力、速度和 Kinematic 命令。
2. `PhysicsSubsystem::PreStep()` 刷新生命周期与 Transform 命令。
3. `PhysicsScene::Simulate()` 调用后端。
4. `PhysicsSubsystem::SyncTransforms()` 只回写 Dynamic Body，并准备渲染插值状态。
5. `PhysicsScene::DrainContactEvents()` 将后端 raw contact 归一化为 Enter/Stay/Exit。
6. `PhysicsSubsystem::DispatchEvents()` 在主线程广播 Collision/Trigger 消息。
7. 普通 `Update` 和 `LateUpdate` 才开始运行。

任何 gameplay callback 都不得从 Jolt `ContactListener` 内直接执行。

### Event semantics

- 统一内部阶段：`Enter`、`Stay`、`Exit`。
- 统一交互类型：`Collision`、`Trigger`。
- contact pair 使用排序后的 engine Body/Collider Handle 作为 key；sub-shape contact 在同一 fixed step 内去重聚合。
- PhysicsScene 产出一次 canonical pair event；EventBus 负责 Scene 级观察，Component/Script 接收由该 pair 投影出的 owner-local convenience callbacks。
- 两个参与方各收到一份 self-oriented callback view，normal 方向与 `self` 一致；Scene observer 不需要对同一 pair 去重。
- Exit 不承诺有效接触点和冲量，使用显式 `hasContactData`。
- Body 被销毁时，销毁对象不再接收 callback；仍存活的对端收到一次可识别的 Exit，然后 pair cache 清理。
- backend 的 Removed 不直接等于 gameplay Exit；sleep/deactivation、真实分离、Body 销毁和 filter change 必须在模拟后分类，避免静止接触因 sleep 产生伪 Exit/Enter。

### Collision filtering

- 使用 32 个可命名 Object Layer。
- Project Physics Settings 保存对称的 `Ignore / Overlap / Block` 响应矩阵和默认 profile。
- Trigger 将非 Ignore 的交互降为 Overlap，不产生物理解算，但可生成事件。
- Scene Query 使用独立 `PhysicsQueryFilter`，支持 layer mask、是否命中 Trigger、忽略 Body/GameObject。
- 不继续保留“descriptor 有 per-body collisionMask，但后端实际只读取全局 mask”的双重语义。

### Handle and threading rules

- 公共 Handle 是 engine-owned index + generation；Jolt BodyID 只保存在 backend registry。
- 创建、销毁、重建、Transform 更新和力命令在 fixed-step 边界统一提交。
- Jolt ContactListener 只能复制必要 POD 数据到线程安全队列，不能访问 Scene、EventBus 或修改 Body。
- 事件发布和 GameObject 查找只发生在主线程。

## Milestones

| Milestone | Steps | Outcome |
| --- | --- | --- |
| M0 Stabilize | 0.1, 0.2 | 后端契约、Runtime RAII、Handle 与 Body 生命周期可靠（Complete） |
| M1 Events | 1.1, 1.2 | Collision/Trigger Enter/Stay/Exit 完整到达游戏层 |
| M2 Authoring | 2.1, 2.2 | Collider/Rigidbody 分离，Transform 与常用运动 API 明确 |
| M3 Filtering & Queries | 3.1, 3.2 | 命名层、响应矩阵和常用 Scene Query 可用 |
| M4 Simulation Features | 4.1, 4.2 | 形状、材质、CCD 和基础约束完善 |
| M5 Gameplay Physics | 5.1 | CharacterController 与 Rigidbody 解耦 |
| M6 Quality Gate | 6.1 | Debug draw、Profiler、回归和性能门禁形成闭环 |

## Recommended implementation order

严格按以下顺序实施：

1. Step 0.1：冻结公共契约和 Runtime ownership。**已完成（2026-07-16）**
2. Step 0.2：修复 Body lifecycle、Handle 和 command buffer。**已完成（2026-07-16）**
3. Step 1.1：建立 raw contact 到稳定 pair state 的转换。
4. Step 1.2：接入 Scene EventBus、Component 和 Script 回调。
5. Step 2.1：拆分 Collider/Rigidbody，并迁移序列化与 Editor。
6. Step 2.2：完成 Transform authority、插值和常用运动方法。
7. Step 3.1：完成 layer/profile/response 配置。
8. Step 3.2：补齐 Raycast、Sweep、Overlap 查询族。
9. Step 4.1：补齐形状、材质、CCD 和 sleep 行为。
10. Step 4.2：实现基础约束。
11. Step 5.1：实现独立 CharacterController。
12. Step 6.1：统一调试、测试、性能和文档验收。

M0-M3 是可用物理层的核心闭环；M4-M5 是常见引擎能力扩展；M6 必须随每个里程碑增量维护，最后集中验收。

## Explicit non-goals

首轮路线不包含：

- Soft Body、cloth、fluid、destruction、vehicle 和 ragdoll authoring。
- 网络物理预测、rollback、跨平台 bit-exact determinism。
- 异步跨帧 Physics Scene 或把 Jolt job 直接暴露给 Framework JobSystem。
- 动态 Triangle Mesh、任意运行时 mesh cooking 和大世界分区物理。
- 在完成核心事件、生命周期和查询前先堆叠高级 Jolt 功能。

这些能力只能在 Step 6.1 的稳定性与性能基线通过后另建卡片。

## System acceptance

- 两个 Body 的 Collision 和 Trigger 都能稳定产生 Enter、Stay、Exit，且每个参与方的 self/other/normal 正确。
- Jolt 回调线程不执行 gameplay 代码；主线程 dispatch 顺序可复现。
- 删除、禁用、重建 Body 不产生悬空访问、重复 Exit 或泄漏。
- Dynamic、Kinematic、Static 的 Transform 权威和 Teleport 行为有测试锁定。
- Layer/Profile/Query Filter 在模拟与查询中语义一致。
- Box、Sphere、Capsule、Trigger、Raycast、Overlap 和 Sweep 有端到端测试。
- Editor 可查看碰撞体、contact、sleep 状态、layer 和最近 query。
- 核心测试可在无窗口 CI 环境运行，不依赖 Editor 人工操作。

## Official references

- [Jolt ContactListener](https://jrouwe.github.io/JoltPhysicsDocs/5.2.0/class_contact_listener.html)：回调并发、Body lock、Removed 阶段缓存约束。
- [Unreal Collision Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/collision-in-unreal-engine---overview?lang=en-US)：Ignore/Overlap/Block 与事件生成分离。
- [Unreal Traces Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/traces-in-unreal-engine---overview)：single/multi hit、query channel 与 shape trace。
- [Godot Physics Layers and Masks](https://docs.godotengine.org/en/stable/tutorials/physics/physics_introduction.html#collision-layers-and-masks)：32 层、layer/mask 与命名配置。

这些资料只用于确定常见语义；ChikaEngine 公共 API 保持后端无关，不复制特定引擎命名或对象模型。
