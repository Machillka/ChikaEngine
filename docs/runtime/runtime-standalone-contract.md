# Standalone Runtime Contract

## Metadata

- Date: 2026-06-15
- Status: Active
- Applies to: `ChikaGame`, Runtime modules, future Cooked/Packaged Runtime

## Dependency Boundary

`ChikaGame` may depend on `ChikaEngine` and `ChikaRuntime`. Runtime modules may depend on platform, asset, framework, render, RHI, resource, input, time, physics and scripting modules.

The following dependencies are forbidden in `engine/Runtime/` and `engine/Game/`:

- `ChikaEditor`, `EditorManager`, Editor Panel or Editor source files.
- ImGui API, ImGui backend API or ImGui-owned descriptor resources.
- UI-framework-specific methods in `IRHIDevice`, `IRHICommandList` or `Renderer`.

`Chika.RuntimeBoundary` scans this rule on every test run.

## Runtime Modes

| Mode | Source assets / Importer / Hot reload | Editor UI | Cooked Registry |
| --- | --- | --- | --- |
| Development Game | Temporarily allowed until Step 3.4 | Forbidden | Optional |
| Editor Development | Allowed | Allowed, owned by Editor | Optional |
| Packaged Game | Forbidden | Forbidden | Required |

Phase 0 proves the executable and module boundary. Source-free startup, Project Descriptor and Cooked Registry remain later phases.

## Failure Contract

- `Application::Run()` returns non-zero when `EngineContext` initialization or application execution fails.
- Smoke mode must use the normal `RequestExit()` and reverse shutdown path.
- Future packaged runtime must fail fast on missing Project Descriptor, Registry, startup Scene or cooked dependency.

## Verification Contract

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- `build/bin/ChikaGame.exe --smoke-frames 3`
- Game-only Release configure/build with `CHIKA_BUILD_EDITOR=OFF`
- Start and normally close `ChikaEditor`
