# Step 3.7 Asset Pipeline Verification

## Increment Goal

Lock the formal asset pipeline contracts into automated integration coverage.

## Covered Contracts

- A new source file receives a `.meta` sidecar.
- GUID remains stable after rebuilding the Asset Database.
- Registered passthrough Importer resolves and imports its asset.
- Asynchronous load returns a usable typed handle.
- Hot reload replaces CPU data without changing that handle.
- Reload subscribers receive one event for the changed source.
- Explicit unload invalidates the typed handle.

## Test

```text
Chika.AssetPipeline
```

The test uses an isolated temporary asset root and removes it when complete. It does not mutate project assets.

## Runtime Verification

Vulkan delayed GPU destruction and disk Pipeline Cache require a real Vulkan device, so they are verified by starting and closing `ChikaEditor` with validation enabled and checking the captured log.

## Verification Result

- `cmake --build build`: passed.
- `ctest --test-dir build --output-on-failure`: 3/3 tests passed.
- `Chika.AssetPipeline` invoked `glslc` on a temporary valid Shader and verified the generated `.spv`.
- `ChikaEditor` ran for ten seconds and exited with code 0.
- Captured editor log contained no Importer errors, Vulkan Validation errors, or VUID messages.
- Pipeline Cache was written successfully.
- Project assets contain 37 source meta files, zero generated `.spv.meta` files, and zero absolute imported paths.
