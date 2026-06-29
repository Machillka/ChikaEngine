# Runtime Stabilization Next Actions

## Metadata

- Date: 2026-06-29
- Scope: Runtime stabilization after dependency/coupling review
- Status: Proposed next actions, no source changes in this pass

## Goal

当前引擎已经有足够多可运行内容，下一阶段重点应该是稳定已有结构，而不是继续堆功能。推荐先做低风险边界收紧和测试护栏，让已有 Gameplay、Render、Asset、Physics、Project startup 路径更可靠。

## Priority 0: Freeze Existing Contracts

先把已经工作良好的契约写成检查，而不是马上改架构。

Recommended checks:

- Extend `tests/cmake/RuntimeBoundaryCheck.cmake` beyond Editor/ImGui:
  - Runtime/Game 继续禁止 `ImGui`、`EditorManager`、Editor panel。
  - `engine/Runtime/Render` 禁止 include `scene/`、`gameobject/`、`component/`。
  - `engine/Runtime/Resource` 禁止 include `Renderer.hpp`、`RenderPipeline.hpp`、`RenderGraph.hpp`。
  - `engine/Runtime/Framework` 禁止 include Vulkan backend headers。
- Add a small dependency note near each runtime `CMakeLists.txt`:
  - Module responsibility.
  - Allowed upstream dependencies.
  - Forbidden dependencies.
- Keep these verification commands as the normal gate:
  - `cmake --build build`
  - `ctest --test-dir build --output-on-failure`
  - `build/bin/ChikaGame.exe --smoke-frames 3`

## Priority 1: Make Real Dependencies Explicit

Low-risk cleanup:

- Add `ChikaAsset` explicitly to `ChikaFramework` public dependencies, because Framework directly includes and uses Asset headers.
- Avoid relying on `ChikaRender -> ChikaAsset` transitive propagation to make Framework compile.
- Consider moving render config enums used by `EngineContextCreateInfo` and `ProjectDescriptor` into a lighter header, for example:

```text
engine/Runtime/Render/include/ChikaEngine/RenderConfig.hpp
```

This can reduce unnecessary inclusion of `Renderer.hpp` from project/application code.

## Priority 2: Reduce Framework Header Coupling

Do this in small steps. Do not rewrite Scene.

Suggested sequence:

1. Forward declare in `scene.hpp` where possible.
   - `Render::Renderer`
   - `Asset::AssetManager`
   - subsystem classes
   - keep concrete includes in `scene.cpp`

2. Move render proxy construction out of component headers.
   - `CameraComponent` should expose camera data.
   - `LightComponent` should expose light data.
   - `RenderSubsystem.cpp` should build `RenderView` and `RenderLightProxy`.
   - Target result: component headers no longer include `RenderWorld.hpp`.

3. Make `MeshRenderer.h` lighter.
   - It can keep `AssetReference` and handles.
   - Forward declare `AssetManager` if only used by reference in method signatures.
   - Keep loading logic in `.cpp`.

4. Keep `RenderSubsystem` as the only gameplay-to-render bridge.
   - It may read `Scene/GameObject/Component`.
   - `Renderer`, `RenderPipeline`, `RenderGraph`, `RHI` must not read gameplay objects.

## Priority 3: Clarify Resource Upload Lifetime

Current behavior depends on Vulkan delayed deletion:

```text
ResourceManager::_UploadMesh/_UploadTexture
    -> create staging buffer
    -> enqueue upload request
    -> DestroyBuffer(staging)
    -> RenderPipeline::AddUploadPasses consumes request later in same frame
```

This works because Vulkan destruction is delayed. It should become an explicit RHI contract before another backend exists.

Options:

- Add an RHI-level comment and test saying `DestroyBuffer` is deferred until safe.
- Or let `ResourceManager` retain staging handles until `RenderPipeline` confirms upload requests were consumed.

The second option is cleaner but slightly more code.

## Priority 4: Stabilize Gameplay-Physics Boundary

Keep current `Rigidbody` behavior, but avoid growing direct physics access from components.

Recommended changes later:

- Hide `PhysicsBodyHandle` behind `PhysicsSubsystem` if more gameplay physics APIs are added.
- Add collision event propagation before exposing collision callbacks to scripts.
- Decide whether default gravity should stay zero for demos or become `{0, -9.8, 0}` for gameplay expectation.
- Add a test for Rigidbody disable/enable destroying and recreating body exactly once.

## Priority 5: Project And Packaged Runtime Boundary

`ProjectDescriptor` already builds different boot configs for Editor, DevelopmentGame and PackagedGame. The next stability step is to verify packaged assumptions:

- DevelopmentGame can scan source content and import.
- PackagedGame must not create content root.
- PackagedGame must not scan source assets.
- PackagedGame must not import or hot reload.
- Startup scene must resolve by GUID.

Add or extend tests around `BuildRuntimeBootConfig` and `AssetManager` initialization flags.

## Priority 6: Document Singletons As A Temporary Contract

These systems are currently global/singleton:

- `InputSystem`
- `TimeSystem`
- `ScriptsSystem`
- `ProfilerSession`
- `UIDGenerator`

For this project stage, that is acceptable. Write the limitation down:

- One engine context per process.
- One input/time/script VM instance per process.
- Tests that need isolation must call `Shutdown()` or reset APIs.

Do not refactor all of them into instance services until there is a real multi-context requirement.

## Suggested Order

1. Add dependency boundary checks.
2. Make `ChikaFramework -> ChikaAsset` explicit in CMake.
3. Remove `RenderWorld.hpp` from camera/light component headers by moving conversion into `RenderSubsystem.cpp`.
4. Forward declare more in `scene.hpp`.
5. Add one Resource upload lifetime test or documented RHI contract.
6. Add packaged runtime config tests.

This order keeps behavior stable while reducing the coupling surface that future work can accidentally depend on.

## Definition Of Done

For a stabilization change in this area, require:

- Existing `cmake --build build` passes.
- Existing `ctest --test-dir build --output-on-failure` passes.
- Runtime boundary check includes the new dependency rule if applicable.
- No new direct Runtime dependency on Editor/ImGui/Vulkan backend outside the existing Editor adapter.
- If a header dependency is removed, the equivalent implementation include is added in `.cpp`.

