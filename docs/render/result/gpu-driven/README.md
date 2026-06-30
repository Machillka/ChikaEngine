# GPU-driven Phase 4 Results

## Status

- Date: 2026-06-26
- Build: Debug
- Hardware observed by benchmark: NVIDIA GeForce RTX 5060 Ti, Windows, 24 logical CPUs
- Result type: Debug correctness gate for the first production GPU-driven consumer, not a Release performance report

## Command

The smoke runs requested GPU mode and saved schema v3 results:

```powershell
.\build\bin\ChikaBenchmark.exe --scene empty --mode gpu --workers 1 --warmup 3 --frames 5 --output build\phase4-gpu-smoke.json
.\build\bin\ChikaBenchmark.exe --scene state-diversity --mode gpu --workers 1 --warmup 3 --frames 5 --output build\phase4-gpu-state-diversity.json
```

## Key Output

- `configuration.requestedMode = "gpu"`
- `configuration.effectiveMode = "gpu"`
- `samples[0].renderStatistics.requestedRenderPath = 2`
- `samples[0].renderStatistics.effectiveRenderPath = 2`
- `samples[0].renderStatistics.renderPathFallback = 0`
- `renderPathFallback = 0` means no fallback.
- Empty scene counters were `gpuDrivenInstanceCount = 0`, `gpuDrivenVisibleCount = 0`, `gpuDrivenDrawGroupCount = 0`, `gpuDrivenIndirectCommandCount = 0`.
- `state-diversity` counters were `gpuDrivenInstanceCount = 10000`, `gpuDrivenVisibleCount = 3159`, `gpuDrivenDrawGroupCount = 4`, `gpuDrivenIndirectCommandCount = 4`.
- `state-diversity` oracle fields were `gpuDrivenValidationCompared = 1`, `gpuDrivenValidationMatched = 1`, `gpuDrivenValidationMissingCount = 0`, `gpuDrivenValidationExtraCount = 0`.

## Interpretation

Phase 4 now has the first real GPU-driven static opaque path: RHI capabilities, GPU-side data ABI, compute shader asset, RenderGraph buffer semantics, reset/cull/graphics pass chain, visible-list shader indirection, indirect submission helper, CPU/GPU oracle, editor diagnostics and benchmark serialization.

The GPU path uploads `GpuInstanceData` and `GpuDrawGroup`, resets indirect args, dispatches `gpu_frustum_cull.comp`, then the graphics pass consumes `visibleInstanceIndices` and `indirectArgs` through `DrawIndexedIndirect`. The benchmark verifies correctness by comparing CPU-visible object ids against the delayed GPU readback/oracle result.

This is still not a production performance claim. The run is Debug, validation/oracle-friendly, and only covers static opaque drawing. Transparent, skinned and shadow paths continue to use CPU queues.

## Remaining Work

- Run Release benchmark matrix for Serial CPU, Job CPU and GPU-driven with 1K/10K/50K/100K scenes.
- Decide Phase 5 based on the measured bottleneck: Hi-Z occlusion if overdraw/visibility dominates, or secondary command recording if CPU command generation dominates.
- Replace the debug-friendly CPU-visible readback path with a configurable staging readback path before publishing final performance numbers.

## Knowledge Notes

- The correctness gate checks the data contract, not just pixels: visible ids, indirect instance counts and fallback state must all agree.
- `DrawIndexedIndirect` still binds one mesh/material/pipeline group on CPU in this phase. That is a deliberate intermediate design; GPU material binning and bindless resources are later architecture steps.
- Keeping CPU oracle beside GPU culling is the practical way to evolve a renderer without losing deterministic validation.
