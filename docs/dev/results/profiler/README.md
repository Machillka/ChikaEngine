# Phase 1 Profiler Results

## Environment and Protocol

- Build: Release, Clang 22.1.8, Windows.
- Command: `build-profiler/bin/ChikaProfilerOverheadBenchmark.exe --iterations 10000 --repeats 15`.
- One stage zone encloses 16,384 deterministic arithmetic work units.
- Each candidate is paired with a baseline sample; order alternates each repeat. The reported overhead is the median of paired percentages.

## Result

| Mode | Median overhead | Gate | Result |
|---|---:|---:|---|
| Compile disabled | -0.215% | Reference | Measurement noise |
| Runtime disabled | 0.029% | < 0.1% | Pass |
| Enabled | 0.517% | < 2.0% | Pass |

`overhead.json` is the raw machine-readable result and includes every baseline/candidate pair. A negative compile-disabled value is normal measurement noise; it is not interpreted as a speedup.

`sample-trace.json` is a bounded 256-zone Trace Event example for Perfetto/Chrome. It demonstrates schema and metadata, while the overhead calculation uses all iterations and remains recorded separately in `overhead.json`.

## Interpretation

- Runtime-disabled initially failed because the gate was checked after reading the monotonic clock. Moving the gate before timestamp acquisition removed work that should never occur while disabled.
- The result only validates stage-level instrumentation. Adding zones around individual objects, draw calls, or tiny arithmetic operations would violate the Phase 1 granularity contract and produce materially higher overhead.
