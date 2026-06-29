# Step 3.3 Asynchronous Loading And CPU Asset Unload

## Increment Goal

Give `AssetManager` an explicit lifetime and allow expensive disk import/load work to run outside the caller thread.

## Design

- `AssetManager::Initialize` owns Asset Database and default Importer initialization.
- Async APIs return `std::shared_future<Handle>`.
- Shutdown rejects new async work and waits for active loads before clearing storage.
- Explicit `Unload` invalidates one typed handle and removes its path cache entry.
- `UnloadAll` clears all CPU-side asset data.
- `SlotMap` uses stable deque storage so appending asynchronous loads does not invalidate existing asset pointers.

## Usage

```cpp
Asset::AssetManager assets;
assets.Initialize("Assets");

auto future = assets.LoadTextureAsync("Assets/Textures/test.png");
const Asset::TextureHandle texture = future.get();

assets.Unload(texture);
assets.Shutdown();
```

## Boundary

This step unloads CPU asset data. GPU resource unloading is implemented separately in the Resource/RHI increment because it must respect GPU fences.
