# Step 3.6 Vulkan Pipeline Cache

## Increment Goal

Persist driver pipeline compilation data across engine sessions.

## Design

- `RHI_InitParams` exposes a pipeline cache path.
- Vulkan loads cache bytes after logical device creation.
- Every graphics pipeline is created with the shared `VkPipelineCache`.
- Shutdown waits for the device, writes cache bytes, then destroys the cache.
- Incompatible cache data falls back to a new empty cache.

## Default Location

```text
.chika/cache/vulkan_pipeline_cache.bin
```

This directory is generated runtime cache data and should not be treated as source assets.

## Boundary

`VkPipelineCache` is driver-specific acceleration. A future engine-level Pipeline Library should additionally deduplicate Pipeline descriptions and dependency graphs before calling RHI.
