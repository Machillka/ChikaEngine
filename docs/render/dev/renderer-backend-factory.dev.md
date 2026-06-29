# Renderer Backend Factory Dev Log

## Metadata

- Date: 2026-06-03
- Area: Runtime/Render
- Related files: `engine/Runtime/Render/include/ChikaEngine/rhi/RHIBackendFactory.hpp`, `engine/Runtime/Render/src/rhi/RHIBackendFactory.cpp`, `engine/Runtime/Render/include/ChikaEngine/Renderer.hpp`, `engine/Runtime/Render/src/renderer.cpp`
- Status: Complete

## Goal

Move `Renderer` away from constructing the Vulkan RHI implementation directly, so backend selection has a single factory entry point.

## Context

`Renderer::Initialize` previously created `VulkanRHIDevice` directly. That made `Renderer` depend on a concrete backend even though most rendering code already talks through `IRHIDevice`.

The existing `RHIBackendFactory.hpp` contained commented-out prototype code and included the Vulkan backend in a public factory header.

## Changes

- Added `RHIBackendFactory::CreateRHIDevice(RHIBackendTypes, const RHI_InitParams&)`.
- Added `engine/Runtime/Render/src/rhi/RHIBackendFactory.cpp` so the factory header no longer exposes `VulkanRHIDevice`.
- Added `RendererCreateInfo::backendType`, defaulting to `RHIBackendTypes::Default`.
- Updated `Renderer::Initialize` to request the backend from `RHIBackendFactory`.
- Removed an unnecessary Vulkan include from `RenderGraph.cpp`.
- Added `docs/dev-template.dev.md` as the required template for future `docs/*.dev.md` development logs.

## Verification

- Commands run: `cmake --build build`
- Result: Success. CMake reconfigured after the new source file was added, compiled `RHIBackendFactory.cpp`, linked `ChikaRender`, and produced `build/bin/ChikaEditor.exe`.
- Remaining warnings or risks: `OpenGL` is intentionally rejected because no OpenGL RHI backend exists in the current runtime path. The build still emits existing third-party VMA nullability warnings and one existing `glfw` include case warning in `engine/Editor/src/main.cpp`.

## Next Steps

- Add an engine-level render settings object so backend selection can come from config rather than hardcoded defaults.
- Split backend-specific editor integration, especially ImGui texture handles and ImGui backend initialization.
- Introduce a small `ChikaRHI` target or equivalent boundary so RHI types can be shared without keeping `Render` and `Resource` tightly coupled.
- Move GPU resource ownership out of `Renderer` into a dedicated render resource system.
