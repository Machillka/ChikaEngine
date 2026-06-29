# Step 3.5 GPU Resource Unload

## Increment Goal

Complete the resource lifetime from CPU asset handles through GPU allocations.

## Design

- `ResourceManager::Unload` releases Mesh, Texture, or Material GPU ownership.
- Material resources now record the Shader and Pipeline handles they created.
- RHI exposes explicit Shader and Pipeline destruction.
- Vulkan schedules Buffer, Texture, Shader, and Pipeline destruction through its frame-delayed deletion queues.
- `ResourceManager` unloads all owned resources before destruction.

## Usage

```cpp
const Resource::TextureHandle texture = resources.UploadTexture(assetTexture);
resources.Unload(texture);
```

CPU assets and GPU resources have separate handles and separate unload calls. Unloading a CPU asset does not implicitly destroy a GPU resource that may still be used by rendering.

## Safety

GPU destruction is deferred beyond frames in flight. Callers must stop submitting a Resource handle after requesting unload.
