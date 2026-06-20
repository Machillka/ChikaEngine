# Job System 与 Task Graph 规划

## Metadata

- Date: 2026-06-19
- Status: Phase 2 Complete (2026-06-20)
- Depends on: Unified Profiler event contract
- Priority: P0

## Goal

建立由 EngineContext 管理生命周期、可表达依赖、可被 Profiler 观察的通用 Job System，并用 Renderer 与 Asset 工作负载证明它不是独立并发练习。

## 当前实现边界

- `ChikaJobs` 已提供有界 Job Storage、generation handle、injection/local queue、work stealing、依赖、Parent/Child、wait-help 和两种关闭策略。
- AssetManager 已移除直接 `std::async`；CPU 读取/解析使用注入 JobSystem，GPU 上传仍留在 Resource/Render 线程边界。
- Renderer 当前只并发主视图和阴影视图两个完整 Visibility 工作；对象级分块、Packet、Sort 仍属于 Phase 3。
- Jolt 保持自己的线程池，首版不强行统一物理调度器。

## 设计原则

- **先正确再无锁。** 首版可以使用有界锁和条件变量；只有 Profiler 证明队列竞争后才引入 Chase-Lev Deque。
- Job 只捕获 CPU 数据，不允许 Worker 随意访问 Vulkan Device 或 Editor 状态。
- Worker 等待 Job 时协助执行其他可运行任务，避免线程池内部互相阻塞。
- External Thread Wait 可以使用 Condition Variable；Main Thread Wait 必须保持事件泵和明确边界。
- 所有任务都有完成语义，Engine Shutdown 不允许遗留后台线程。

## 核心 API 草案

```cpp
JobHandle Schedule(JobDesc job);
JobHandle ScheduleAfter(std::span<const JobHandle> dependencies, JobDesc job);
void ParallelFor(uint32_t count, uint32_t grainSize, Function function);
void Wait(JobHandle handle);
bool IsComplete(JobHandle handle) const;
```

`JobHandle` 使用 index + generation，不能持久化裸指针。Job Storage 使用 Pool，避免每任务单独堆分配。

## 调度结构

```text
External/Main Thread
    -> Global Injection Queue

Worker N
    -> Local Deque (LIFO local execution)
    -> Steal other Worker (FIFO steal)
    -> Sleep/Wake Semaphore

Job Completion Counter
    -> Dependent Job readiness
    -> Waiter notification
```

首版任务类型：

- `AnyWorker`：普通 CPU Job。
- `MainThread`：必须回到主线程提交的完成任务。
- `IO` 不是首版独立线程池；阻塞文件 IO 在后续根据 Profiler 决定是否隔离。

## 最小步骤

1. 定义 JobSystem CreateInfo、Worker 生命周期和线程命名。
2. 实现 Job Pool、Global Queue、Worker Local Queue、Sleep/Wake。
3. 实现 Handle、Completion Counter、Parent/Child 和多依赖 Continuation。
4. 实现 Worker `Wait-help`、External Wait 和递归调度。
5. 实现 `ParallelFor`、合理 Grain Size 和确定性合并辅助器。
6. EngineContext 统一 Initialize/Shutdown；增加 Drain 和 Cancel Pending 策略。
7. 接入 Profiler：Queued、Running、Waiting、Steal、Worker Utilization。
8. 替换 AssetManager 零散 `std::async`，但保持 GPU Upload 在正确线程执行。
9. 接入 Renderer 的纯数据 Visibility/Packet 工作负载。

## 正确性测试

- 100 万短任务无丢失、无重复执行。
- Parent/Child、多前置依赖、Fan-in/Fan-out 顺序正确。
- Worker 内 Wait 不死锁，单 Worker 配置也可完成。
- Job 中继续 Schedule 能正确完成。
- Shutdown Drain 完成已接收任务；Cancel 模式让 Future/Handle 明确失败。
- Handle generation 能拒绝已回收 Job 的旧句柄。
- Worker 异常被捕获并传播，不终止整个进程。
- 重复启动/关闭和 Engine 初始化失败回滚不泄漏线程。

## 性能验收

- 记录 1/2/4/8 Worker 吞吐和扩展曲线。
- 分别测试 1 us、10 us、100 us、1 ms 任务，说明调度适用粒度。
- 统计 Queue Wait、Execution、Steal、Sleep 和 Main Thread Wait。
- 与串行、`std::async` 和单全局队列版本对比。
- 不以“无锁”作为成功标准，以真实工作负载 Frame Time 为标准。

## 作品集交付

- Scheduler 架构图与 Job 状态机。
- 并发正确性测试说明。
- Worker Timeline 和 Utilization 截图。
- Renderer/Asset 实际接入前后数据。
- 一段解释 Wait-help、Shutdown 和 generation handle 的技术说明。

## 非目标

- 首版不实现 Fiber Scheduler。
- 首版不统一替换 Jolt 内部 Job System。
- 首版不做复杂 Priority Inheritance、NUMA 和跨进程调度。
- 不在没有数据支持时实现 Lock-free MPMC 结构。

## Phase 2 Delivery Summary

```text
EngineContext
  -> JobSystem (owns workers and bounded storage)
  -> AssetManager (submits CPU load jobs)
  -> Renderer (submits read-only visibility jobs)

External submit -> Injection FIFO
Worker child    -> Local LIFO
Idle worker     -> Remote FIFO steal -> Condition Variable sleep
Completion      -> Dependents / Parent counter / Waiter notification
```

- 正确性：100 万短任务无丢失/重复；Chain、Diamond、Fan-in/Fan-out、异常、Cancel、Drain 和 generation 测试通过。
- 可观测性：Worker lane、Job Zone、因果 instant、queue/worker counter 和 Perfetto payload 已接入统一 Profiler。
- 性能：原始 Release 结果位于 `docs/dev/results/jobs/scheduler.json`；当前有锁实现的合理粒度约从 100 us 开始，不能用于 1 us 碎任务。
- 下一步：Phase 3 先建立串行 Render oracle 和 data-oriented instance view，再做对象级 Parallel Visibility；不要继续扩大 Phase 2 的调度 API。

