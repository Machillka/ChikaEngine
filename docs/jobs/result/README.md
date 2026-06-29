# Phase 2 Job Scheduler Results

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
