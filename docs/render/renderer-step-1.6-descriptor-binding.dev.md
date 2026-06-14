# Renderer Step 1.6：Layout-aware Descriptor Binding

## Metadata

- Date: 2026-06-13
- Status: Complete
- Main files: `ResourceBinder.cpp`, `VulkanCommandList.cpp`, `VulkanRHIDevice.cpp`

## Goal

按目标 Layout 分配、校验和缓存 Descriptor Set，停止每个 Draw 盲目更新完整全局 Set。

## Implementation

- 每帧 Pool 分配 Transient Descriptor；独立 Pool 保存 Persistent Material Descriptor。
- 持久 Descriptor 按 Layout 和资源内容 Hash 缓存，相同 Material 可跨 Draw 复用。
- 写入前校验 set、binding、类型和数组元素。
- 名称只在创建阶段解析为 `ResourceBindingHandle`，逐 Draw 不搜索 Reflection。
- Descriptor Pool 覆盖 Uniform/Storage Buffer、Image 和 Sampler 类型。

## Statistics

| Pipeline | Frame | Descriptor Writes Before | Descriptor Writes After |
|---|---|---:|---:|
| Forward | First | 20 | 14 |
| Forward | Steady | 20 | 10 |
| Deferred | First | 24 | 16 |
| Deferred | Steady | 24 | 12 |

Pass、Draw、Instance 和 Pipeline Bind 数量保持 Phase 0 基线不变。

## Remaining Boundary

持久 Descriptor Cache 当前由 Device 生命周期统一回收；后续资源依赖图可增加精确失效和容量策略。
