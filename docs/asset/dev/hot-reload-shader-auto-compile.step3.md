# Step 3.4 Hot Reload And Automatic Shader Compilation

## Increment Goal

Turn imported assets into a live development pipeline without invalidating existing asset handles.

## Design

- `AssetDatabase::PollChangedAssets` detects source timestamp changes.
- Changed sources run through their registered Importer first.
- `ShaderImporter` recompiles changed shader source through `glslc`.
- `AssetManager::TickHotReload` reloads changed imported data into the existing SlotMap entry.
- Reload subscribers receive GUID, type, and source path.
- `ResourceManager` subscribes to reload events and invalidates GPU caches so the next render submission rebuilds them.
- `EngineContext` polls hot reload at a throttled 500 ms interval.

## Usage

```cpp
const size_t subscription = assets.SubscribeReload(
    [](const Asset::AssetReloadEvent& event)
    {
        // Invalidate dependent GPU resources or editor previews.
    });

assets.TickHotReload();
assets.UnsubscribeReload(subscription);
```

## Handle Contract

Hot reload preserves the existing CPU asset handle. Explicit unload invalidates it. GPU resources that copied old CPU data must subscribe and rebuild their dependent resources.
