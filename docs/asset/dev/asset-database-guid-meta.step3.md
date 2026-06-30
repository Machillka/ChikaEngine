# Step 3.1 Asset Database, GUID And Meta

## Increment Goal

Introduce stable asset identity before changing loading behavior. Paths remain locations; GUIDs become identities.

## Design

- `AssetDatabase` owns the index for one asset root.
- Every source asset receives a sidecar `<asset>.meta`.
- Meta stores schema version, stable GUID, asset type, importer name, and imported artifact path.
- Database lookup supports both GUID and normalized absolute path.
- Generated files and cache directories such as `.meta`, `.pyc`, and `__pycache__` are excluded.
- A `.spv` file with an adjacent Shader source is treated as an imported artifact and does not receive a second GUID.

## Usage

```cpp
Asset::AssetDatabase database;
database.Initialize("Assets");

const auto* record = database.FindByPath("Assets/Textures/test.png");
const auto* sameAsset = database.FindByGuid(record->guid);

Asset::TextureHandle texture = assets.LoadTexture(record->guid);
```

Moving an asset should preserve its `.meta` file so the GUID remains stable.

## Boundary

This increment does not load CPU or GPU resources. It establishes the source-of-truth index used by later Importer, asynchronous loading, and hot-reload steps.
