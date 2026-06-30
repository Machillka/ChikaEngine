ca, CPU-side resource state, GPU upload state, hot reload, and cooked/runtime loading should be documented as separate concepts.
- Agents should know whether a change belongs in importer code, runtime resource code, registry code, or editor project code.

Suggested contract focus:

- Asset handle and path stability.
- Load, cache, reload, unload, and GPU upload lifecycle.
- Cooked asset expectations.
- Rules for adding a new asset type or importer.
- Verification for editor and headless runtime paths.

### Jobs / Profiling

AI-friendly target:

- Job ownership, waiting, cancellation, shutdown, and profiler scope expectations should be written down.
- A new agent should know which APIs are safe from worker threads and which are main-thread-only.

Suggested contract focus:

- JobSystem startup/shutdown.
- Capturing engine objects inside jobs.
- Blocking waits and frame-end synchronization.
- Required profiler zones for long-running work.

### Physics

AI-friendly target:

- Physics should have clear transform sync rules and body-handle ownership rules.
- Gameplay code should not need to know backend-specific physics details.

Suggested contract focus:

- Body creation/destruction.
- Transform read/write authority.
- Fixed-step vs variable-step update.
- Collision/query result lifetime.
- Scene integration points.

### Editor

AI-friendly target:

- Editor UI should be documented as an adapter over engine services, not as a second owner of runtime state.
- Adding a panel or tool should have a playbook that lists data ownership, undo/redo implications, and runtime/editor mode behavior.

Suggested contract focus:

- Editor-only dependencies.
- Project open/save behavior.
- Scene inspection and mutation rules.
- Asset browser and importer interaction.
- Render viewport ownership.

## Development Playbooks To Add

Short playbooks are more useful to AI agents than broad essays. Recommended first playbooks:

- Add a component: files to touch, registration path, transform rules, serialization expectations, tests.
- Add a render pass: render graph resources, pipeline integration, debug naming, frame validation, smoke scene.
- Add an asset type: metadata, importer, runtime resource, cache invalidation, editor exposure.
- Add an RHI feature: public abstraction, Vulkan implementation, synchronization, backend-neutral tests.
- Add an editor panel: UI registration, data ownership, selection state, command/undo rules.
- Add a benchmark scene: scene construction, measurement target, warmup, output contract.
- Change engine startup: initialization order, service ownership, shutdown checks, smoke commands.

Each playbook should include:

- Read first.
- Touch these files.
- Do not touch these files.
- Required checks.
- Common failure signatures.

## Verification Matrix

The project should keep a central verification matrix so agents can choose checks by change type:

| Change type | Minimum verification |
| --- | --- |
| C++ engine code | `cmake --build build` |
| CMake target/dependency | configure plus `cmake --build build` |
| Runtime boundary | `RuntimeBoundaryCheck` and focused unit tests |
| Render graph/pass | render smoke test, baseline scene, GPU validation if available |
| RHI/Vulkan backend | editor launch smoke, benchmark smoke, validation-layer run if available |
| Asset/import pipeline | importer test, project load test, cooked/runtime path if affected |
| Framework/component | scene integration test, game runtime smoke |
| Editor UI | editor launch smoke and affected panel workflow |
| Documentation only | docs build or markdown lint if available |

If a check is too slow or environment-specific, document the lighter fallback and the reason.

## Boundary Guards

AI-friendly projects benefit from automated coupling checks. Recommended guards:

- Extend runtime boundary checks to cover forbidden includes by module.
- Generate a CMake target dependency snapshot and diff it in review.
- Add a lightweight script that reports public headers including heavy backend headers.
- Add a check for editor-only includes leaking into runtime modules.
- Add a check for render backend details leaking into gameplay-facing headers.
- Keep third-party and generated paths excluded from agent edits by policy and tooling.

The first guard should protect the current highest-risk edge: `Framework` depending too broadly on render, physics, asset, and editor-facing implementation details.

## Documentation Conventions For Agents

Use consistent markers so agents can search the repository reliably:

- `AI-ENTRY`: documents a recommended starting point.
- `MODULE-CONTRACT`: marks module responsibility and dependency rules.
- `PLAYBOOK`: marks task-specific instructions.
- `VERIFY`: marks required commands.
- `LIMITATION`: marks known incomplete or intentionally temporary behavior.
- `DO-NOT-TOUCH`: marks generated, vendored, or dangerous files.

These markers should be sparse. They are useful only when they point to authoritative places.

## API Comment Priorities

Do not comment every function. Focus comments where wrong assumptions cause engine bugs:

- Ownership transfer.
c
- Thread affinity.
- Frame-in-flight behavior.
- Shutdown ordering.
- GPU resource validity.
- Scene/component registration.
- Transform synchronization.
- Runtime mode differences.

Short comments near public APIs are enough when detailed explanations live in module contracts.

## Suggested Rollout

P0: Create stable docs entry points.

- Add `docs/index.md`.
- Link `AGENTS.md`, build instructions, runtime architecture notes, render notes, and `docs/reason/` analyses.
- Add a short "Start here for AI agents" section.

P1: Add module contracts.

- Start with `EngineContext`, `Framework`, `Render`, `RHI`, `Resource`, `Asset`, `Jobs`, `Physics`, and `Editor`.
- Keep each contract under one page at first.

P2: Add playbooks for common tasks.

- Prioritize component, render pass, asset type, RHI feature, editor panel, benchmark scene, and startup changes.

P3: Add machine-checkable boundary guards.

- Expand `RuntimeBoundaryCheck`.
- Add include/dependency reports.
- Document allowed exceptions.

P4: Consolidate next actions.

- Move scattered TODOs into a central prioritized list.
- Keep source TODOs only when they are local and immediately actionable.

## Definition Of Done For AI-Friendly Changes

A change that claims to improve AI-friendliness should satisfy at least one of these:

- Reduces the number of files an agent must inspect before making a common change.
- Converts an implicit module rule into an explicit contract.
- Adds a verification command for a class of changes.
- Prevents a known coupling mistake with a test or static check.
- Documents a current limitation in a place agents are likely to read.
- Makes failure output easier to map back to a subsystem.

The best improvements are small and cumulative. The engine does not need a large documentation rewrite; it needs stable landmarks and enforceable boundaries.

## Immediate Next Actions

1. Add a docs index with reading order for humans and agents.
2. Add module contract pages for `EngineContext`, `Framework`, and `Render` first.
3. Create playbooks for adding a component and adding a render pass.
4. Extend runtime boundary checks around `Framework` public headers.
5. Add a verification matrix and keep it current as tests evolve.
6. Consolidate scattered TODOs into a prioritized `next-actions` document.
