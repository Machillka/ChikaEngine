# Phase 0 Serial Benchmark Results

## Protocol

- Build: Release, Clang 22.1.8, ChikaEngine 0.0.1
- Renderer: Vulkan Forward, 1280x720, hidden window, VSync Off
- Run: serial, 1 worker, seed 803274, 300 warmup, 1000 samples, 3 GPU flush frames
- Matrix: `instance-1k`、`instance-10k`、`instance-50k`，每项三次

运行示例：

```powershell
.\build-benchmark\bin\ChikaBenchmark.exe --scene instance-10k --mode serial --workers 1 --warmup 300 --frames 1000 --output docs\dev\results\baseline\instance-10k-run1.json
```

## Files

- `hardware.json`：本次采集机器与工具链。
- `serial-baseline.md`：不修改原始数据的汇总与瓶颈判断。
- `instance-*-run{1,2,3}.json`：Schema v1 原始逐帧结果。
- `smoke-empty.json`：入口、生命周期和结果写入 smoke，不参与正式矩阵。

结果只有在 `schemaVersion`、`sceneConfigHash`、GPU、Build 和 configuration 全部一致时才可比较。JSON 保留所有样本；不得手工删除长尾帧。
