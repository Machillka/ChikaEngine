# Step 3 Formal Asset Pipeline Summary

## Completed Data Flow

```text
Source Asset
  -> AssetDatabase (GUID + meta)
  -> ImporterRegistry (format ownership)
  -> Imported Artifact
  -> AssetManager (CPU handle, async load, hot reload, unload)
  -> ResourceManager (GPU handle, rebuild on reload, unload)
  -> RHI (deferred destruction, Vulkan Pipeline Cache)
```

## Increment Guides

1. `asset-database-guid-meta.step3.md`
2. `importer-registry.step3.md`
3. `async-loading-resource-unload.step3.md`
4. `hot-reload-shader-auto-compile.step3.md`
5. `gpu-resource-unload.step3.md`
6. `vulkan-pipeline-cache.step3.md`
7. `asset-pipeline-verification.step3.md`

## Current Contracts

- GUID is stable as long as the sidecar meta moves with its source asset.
- Meta paths are relative to the asset root and portable across workspaces.
- Imported Shader binaries do not receive duplicate source GUIDs.
- Path loading remains compatible, while GUID loading is available for future serialized references.
- Async loading rejects new work during shutdown and shutdown waits for active work.
- CPU hot reload preserves Asset handles.
- GPU caches are invalidated on asset reload and rebuilt through existing render submission paths.
- GPU resource destruction is deferred beyond frames in flight.
- Vulkan driver Pipeline Cache persists outside source assets under `.chika/cache`.

## Next Maturity Steps

- Replace one-task-per-async-load scheduling with a bounded job system and main-thread commit queue.
- Build an explicit asset dependency graph so hot reload invalidates only affected GPU resources.
- Move imported artifacts from adjacent source files into a versioned Library directory.
- Add Editor Asset Browser, import status, reimport controls, and diagnostics.
- Migrate Scene, Prefab, Material, and Component serialized references from paths to GUIDs.
- Add engine-level Pipeline description deduplication above the driver Pipeline Cache.
