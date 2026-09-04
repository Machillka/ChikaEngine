# Job Scheduler Results

## 2026-08-23 Correctness Audit and Shutdown Wake Fix

### Scope and Environment

- Scope: diagnose the shutdown hang, apply the smallest Jobs implementation fix, and retain bounded regression diagnostics. No module, public API, or third-party dependency was added.
- Revision under test: `7b49b42` on branch `fix`, with unrelated pre-existing workspace changes excluded from this audit.
- Build: Debug, Apple clang 17.0.0, Darwin 24.6.0 arm64.
- Test executable enhancements: named case selection, in-process repetition, shutdown watchdog diagnostics, and separate CTest entries for the three shutdown contracts.

### Reproduction Commands

```bash
cmake --build build/debug --target \
  ChikaJobSystemTests ChikaJobStressTests ChikaAssetJobIntegrationTests -j 4

ctest --test-dir build/debug -R '^Chika\.JobSystem$' --output-on-failure
ctest --test-dir build/debug \
  -R '^Chika\.JobSystem\.(IdleWake|ShutdownCancel|ShutdownDrain|ShutdownRace)$' \
  --output-on-failure
ctest --test-dir build/debug -R '^Chika\.JobStress$' --output-on-failure
ctest --test-dir build/debug -R '^Chika\.AssetJobs$' --output-on-failure

build/debug/bin/ChikaJobSystemTests shutdown-drain 1000
build/debug/bin/ChikaJobSystemTests shutdown-cancel 1000
build/debug/bin/ChikaJobSystemTests shutdown-race 1000
build/debug/bin/ChikaJobSystemTests idle-wake 100
```

The repeat count is an optional second argument. A shutdown taking longer than five seconds aborts the test and prints scheduler counters, so CI reports the failing contract instead of hanging indefinitely.

### Pre-fix Results

| Test | Result | Evidence |
| --- | --- | --- |
| Full JobSystem unit suite, one run | Pass | 1/1, 0.22 s |
| Shutdown subcases, one run each | Pass | 3/3, 0.04 s total |
| Exact-once stress | Pass | 1,000,000/1,000,000 jobs executed exactly once; CTest 4.14 s |
| Asset job integration | Pass | Async success/failure/shutdown integration checks; CTest 0.37 s |
| Submission/shutdown race, 1,000 repetitions | Pass | All 1,000 repetitions completed |
| Drain shutdown, up to 1,000 repetitions | **Fail** | Watchdog fired at repetition 255 |
| Cancel-pending shutdown, up to 1,000 repetitions | **Fail** | Watchdog fired at repetition 806 |

Drain failure counters at the watchdog deadline:

```text
submitted=1000; completed=1000; failed=0; cancelled=0; queued=0; active=0
```

Cancel-pending failure counters at the watchdog deadline:

```text
submitted=101; completed=1; failed=0; cancelled=100; queued=0; active=0
```

The failures are lifecycle hangs after every accepted job has reached a terminal result, not lost or duplicated task execution. An LLDB capture at the drain failure showed:

- main thread blocked in `JobSystem::Impl::Shutdown()` while joining a worker (`JobSystem.cpp:137`);
- the remaining worker blocked in `WorkerMain()` on `m_wakeCondition.wait(...)` (`JobSystem.cpp:474`);
- no queued or active work at the time of capture.

The implementation inspection and stacks confirmed a lost notification window: the worker tested the atomic predicate while waiting under `m_wakeMutex`, while shutdown stored the predicate and notified without participating in that mutex protocol. The same protocol gap existed for the `readyJobs` predicate used to wake an idle worker.

### Fix

- Normal shutdown and worker-start rollback now acquire `m_wakeMutex` while publishing `stopRequested=true`, then call `notify_all()` after releasing the mutex.
- Any-worker enqueue now acquires the same mutex while publishing the ready-job count, then calls `notify_one()` after releasing it.
- Worker wait keeps testing `stopRequested || readyJobs > 0` while holding `m_wakeMutex`. Predicate publication and the transition into `condition_variable::wait` therefore use one synchronization protocol: a producer can no longer notify between the worker's false predicate check and the atomic unlock-and-wait transition.
- A new idle-worker test repeatedly waits until the only worker reports sleeping, submits work, and observes completion without calling `Wait`; this prevents main-thread wait-help from masking a lost ready-job notification.

### Post-fix Results

| Build / test | Result | Evidence |
| --- | --- | --- |
| Debug focused CTest | Pass | 7/7: unit, idle wake, three shutdown cases, one-million-job stress, Asset integration |
| Release focused CTest | Pass | 7/7, including one-million-job stress |
| Debug full repository CTest | Pass | 33/33 |
| Drain shutdown | Pass | Debug 2,000/2,000; Release 2,000/2,000 |
| Cancel-pending shutdown | Pass | Debug 1,000/1,000; Release 1,000/1,000 |
| Submission/shutdown race | Pass | Debug 2,000/2,000; Release 2,000/2,000 |
| Fully idle worker wake | Pass | Debug and Release each ran 100 repetitions of 1,000 no-help wakeups |
| ThreadSanitizer | Pass | Entire JobSystem unit matrix repeated 10 times; no TSan report |

One attempted 10,000-iteration single-process drain run was killed with exit 137 after creating tens of thousands of worker threads; it did not fire the deadlock watchdog. Because the test process also accumulates profiler/thread-registry state across recreated pools, the bounded 2,000-iteration runs plus independent Debug/Release, full CTest, and TSan runs are the valid evidence reported above.

### Verdict

- Core execution correctness is well supported for the exercised scope: exact-once execution, handle generations, exception transport, DAG/fan-in/diamond ordering, failure policies, parent/child completion, main-thread jobs, deterministic `ParallelFor`, stealing, profiler integration, and a real Asset job workload passed.
- Throughput usability for short independent work is demonstrated by the one-million-job run.
- The reproduced `Drain` and `CancelPending` lifecycle hang is fixed for the exercised Debug, Release, and TSan matrix. No post-fix watchdog fired.
- The scheduler is usable for the tested lifecycle and workload contracts. As with any concurrency fix, repetition and sanitizer success reduce risk but do not prove the absence of every possible race.

### Remaining Work

- Run the new named cases in Windows and Linux CI; current fix verification is macOS arm64 only.
- Keep the watchdog and named CTest entries as regression diagnostics; they turn any future silent hang into a bounded, attributable failure.
- If much longer lifecycle soak tests are needed, launch repetitions in fresh processes or bound profiler thread-registry retention instead of recreating tens of thousands of pools in one process.

---

## Phase 2 Performance Results

## Run Contract

- Date: 2026-06-20
- Build: Release, profiling compiled in but capture disabled
- Logical hardware threads: 24
- Workload: 128 independent jobs, 5 repetitions, median reported
- Modes: serial, `std::async`, ChikaJobs with 1/2/4/8 scheduler Workers
- Raw data: `scheduler.json`

```powershell
cmake -S . -B build-jobs -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCHIKA_BUILD_GAME=OFF -DCHIKA_BUILD_EDITOR=OFF `
  -DCHIKA_BUILD_TOOLS=OFF -DCHIKA_BUILD_BENCHMARKS=ON -DBUILD_TESTING=OFF
cmake --build build-jobs --target ChikaJobBenchmark -j 4
.\build-jobs\bin\ChikaJobBenchmark.exe --tasks 128 --repeats 5 `
  --output docs\dev\results\jobs\scheduler.json
```

## Median Results

| Grain | Serial | Jobs 1 | Jobs 2 | Jobs 4 | Jobs 8 | Jobs 8 Speedup |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 us | 0.128 ms | 0.655 ms | 0.542 ms | 0.576 ms | 0.518 ms | 0.25x |
| 10 us | 1.281 ms | 1.702 ms | 1.249 ms | 1.207 ms | 1.196 ms | 1.07x |
| 100 us | 12.894 ms | 13.323 ms | 7.066 ms | 4.205 ms | 3.292 ms | 3.92x |
| 1000 us | 128.000 ms | 128.562 ms | 64.854 ms | 33.427 ms | 18.165 ms | 7.05x |

## Interpretation

- 1-10 us 工作会被当前 Mutex Queue、Handle 和依赖管理成本吞没，应合并 Grain 或走串行路径。
- 100 us 开始得到有效扩展；1 ms 在 8 Worker 下接近 7x，证明调度器适合资产解析和较粗 Renderer CPU 工作。
- Benchmark 使用 completion Job + condition variable 阻塞主线程，主线程不进入 wait-help，因此 Worker 数没有被隐藏增加。
- `std::async` 数据保留为平台实现参考，但它可能由标准库线程池优化，不能当作固定“一任务一线程”模型。
