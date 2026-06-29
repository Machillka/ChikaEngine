# Renderer Step 1.1：Shader Interface 数据模型

## Metadata

- Date: 2026-06-13
- Status: Complete
- Main files: `Core/include/ChikaEngine/shader/ShaderInterface.hpp`, `Core/src/shader/ShaderInterface.cpp`, `ShaderInterfaceTests.cpp`

## Goal

建立不依赖 Vulkan 的 Shader 接口契约，让 Asset、RHI、Material 和测试消费同一份布局信息。

## Core Placement

- 公共接口位于 `Core/include/ChikaEngine/shader/ShaderInterface.hpp`。
- 实现位于 `Core/src/shader/ShaderInterface.cpp`。
- `shader/` 与 Core 中现有的 `math/`、`reflection/`、`serialization/` 一样表示后端无关的基础领域。
- SPIR-V 提取仍属于 Asset 模块；Vulkan Layout 创建仍属于 RHI 模块，避免 Core 依赖导入工具或图形后端。

## Implementation

- 新增 `ShaderReflectionData`、`ShaderProgramInterface`、资源绑定、Buffer Member、Push Constant、输入输出模型。
- Descriptor Type 和 Shader Stage 使用后端无关枚举。
- `NormalizeShaderReflection()` 对资源、成员和接口变量稳定排序。
- 使用显式小端 FNV-1a 字段编码生成跨运行稳定 Hash，不依赖 `std::hash` 或结构体内存布局。

## Verification

- `Chika.ShaderInterface` 验证排序、比较、Hash 稳定性与冲突检测。
- 当前静态、蒙皮、GBuffer、Deferred Shader 均可由数据模型完整表达。

## Design Result

Shader 接口成为独立契约；Vulkan 类型不再向 Asset 和 Material 层泄漏。
