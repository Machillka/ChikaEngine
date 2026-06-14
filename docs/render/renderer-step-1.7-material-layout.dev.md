# Renderer Step 1.7：Reflection Material Parameter Layout

## Metadata

- Date: 2026-06-13
- Status: Complete
- Main files: `ResourceManager.cpp`, `AssetLayouts.hpp`, Material Template JSON

## Goal

让 Material CPU 数据严格按 Shader Buffer 的真实 offset 和 size 写入。

## Implementation

- `ResourceManager` 从反射资源 `material` 获取 UBO 大小和成员 offset。
- 参数写入前校验模板类型与反射成员类型。
- 删除按参数名字典序和手工 std140 对齐计算的旧路径。
- Shader Template Texture 只保留名称，不再保存 slot。
- Material 参数由 `Base.Color` 迁移为与 Shader 成员一致的 `BaseColor`。

## Verification

- Reflection 显示 `material.BaseColor` 位于 offset 0、size 16。
- Forward 与 GBuffer 使用同一持久 Material Binding，Editor 运行结果稳定。

## Design Result

Shader 调整成员顺序或 binding 后，C++ 不需要同步维护 slot 和内存偏移。
