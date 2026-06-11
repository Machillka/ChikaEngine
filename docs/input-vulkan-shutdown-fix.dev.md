# Input And Vulkan Shutdown Validation Fix

## Metadata

- Date: 2026-06-09
- Area: Runtime/Input/Editor/RHI/Docs
- Related files:
  - `engine/Runtime/Input/include/ChikaEngine/GlfwInputBackend.h`
  - `engine/Runtime/Input/src/GlfwInputBackend.cpp`
  - `engine/Editor/src/VulkanAdapter.cpp`
- Status: Complete

## Goal

Remove GLFW invalid mouse button errors and ensure ImGui Vulkan resources are not destroyed while submitted command buffers still reference them.

## Context

The GLFW input backend allocated state for all eight GLFW mouse buttons, but converted only left, right, and middle buttons. Polling the remaining entries passed `-1` to `glfwGetMouseButton`.

Application shutdown calls editor shutdown before renderer shutdown. The editor therefore destroyed ImGui Vulkan buffers, descriptor sets, and pipelines before the renderer's existing GPU idle barrier ran.

## Changes

- Poll GLFW mouse buttons using their native valid index range.
- Added bounds checks to mouse button state queries.
- Removed duplicate left, right, and middle button polling.
- Added an explicit RHI GPU idle barrier before `ImGui_ImplVulkan_Shutdown`.

## Verification

- Commands run:
  - `cmake --build build`
  - `ctest --test-dir build --output-on-failure`
  - Start and close `build/bin/ChikaEditor.exe` while capturing output
  - `git diff --check`
- Result:
  - Full build completed successfully.
  - `Chika.CoreBoundary` and `Chika.SceneIntegration` passed.
  - Editor ran for eight seconds and exited with code 0.
  - Captured editor standard error was empty; no GLFW invalid mouse button or Vulkan validation errors were reported.
- Remaining warnings or risks:
  - `WaitIdle` is intentionally used only at editor backend shutdown. Runtime resource destruction should continue using fence-backed deferred deletion.

## Next Steps

- Add a reusable RHI shutdown contract for future editor or tooling GPU backends.
- Replace frame-count-based runtime deletion maturity with explicit completed-fence tracking.
