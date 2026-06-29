# Renderer Step 1.2：Shader Reflection 导入

## Metadata

- Date: 2026-06-13
- Status: Complete
- Main files: `ShaderReflection.hpp/.cpp`, `AssetImporter.cpp`, `ShaderLoader.cpp`

## Goal

在导入阶段使用 SPIRV-Reflect 提取并持久化接口，Runtime 不重复解析 Shader 源码或 SPIR-V。

## Implementation

- Shader 编译后生成同路径 `.spv.reflection.json`。
- Sidecar 包含 Stage、Descriptor、Buffer Offset、Push Constant、输入输出和稳定 Hash。
- Reflection Sidecar 被 `AssetDatabase` 忽略，不创建独立 GUID；它仍属于源 Shader 的导入产物。
- Material Template 和 Renderer 引用 `.vert/.frag` 源资产路径，由 `AssetManager` 解析到对应 SPIR-V。
- `ShaderData` 加载 SPIR-V 时同步加载 Reflection。
- Sidecar 缺失、损坏或 Hash 过期时，Importer 使用现有 SPIR-V 自动重建。

## Verification

- `Chika.AssetPipeline` 验证编译、Sidecar 生成、加载和逻辑资产身份。
- 修改 Shader 后，Editor 启动会重新编译并同步更新 Reflection。

## Remaining Boundary

外部直接提供且没有 Sidecar 的 `.spv` 仍可作为旧资产加载，但不能创建 Reflection Pipeline。
