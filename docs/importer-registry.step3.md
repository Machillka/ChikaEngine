# Step 3.2 Importer Registry And Shader Import

## Increment Goal

Separate source discovery from format-specific import work.

## Design

- `IAssetImporter` is the import boundary.
- `ImporterRegistry` resolves the importer name stored in asset meta.
- `PassthroughImporter` validates assets consumed directly from source files.
- `ShaderImporter` invokes `glslc` only when the source is newer than its `.spv` output.
- Successful imports update the imported artifact path in the asset meta.

## Usage

```cpp
auto importers = Asset::ImporterRegistry::CreateDefault();
const auto* shader = database.FindByPath("Assets/Shaders/test.vert");
const Asset::ImportResult result = importers.Import(database, shader->guid);
```

Custom importers register by stable name. Meta files reference that name, which allows importer implementations to change without changing asset GUIDs.

## Boundary

Importer execution is synchronous in this increment. Asynchronous scheduling is added in the next step, while import format ownership remains here.
