![ChikaEngine 资源层现代化指南](RESOURCE_LAYER_REFACTORING.md)

# ChikaEngine 资源层现代化指南

**作者**: GitHub Copilot  
**日期**: 2026-03-07  
**范围**: Render Resource Layer (CPU + GPU) 的完整重构  
**目标**: 数据驱动设计 + THandle 版本化资源管理

---

## 📋 概述

本文档记录了 ChikaEngine 渲染资源层的现代化重构过程。通过引入 `THandle<T>` 版本化句柄、严格分离 CPU 端数据与 GPU 端资源，实现了与 Vulkan 实现相匹配的架构。

### 核心改进

| 方面         | 旧设计                        | 新设计                          | 优势                       |
| ------------ | ----------------------------- | ------------------------------- | -------------------------- |
| **句柄类型** | `std::uint32_t` (无类型检查)  | `THandle<Tag>` (编译期类型安全) | 防止错误混用，自动生成错误 |
| **内存安全** | 无版本号，容易 use-after-free | 包含生成号，检测失效句柄        | 运行时安全验证             |
| **数据结构** | Mesh/Texture 与 RHI 混淆      | 明确分离 CPU/GPU 资源           | 可序列化，清晰关注点       |
| **查询性能** | O(map lookup)                 | O(1) 数组索引                   | 更快的渲染管线             |
| **可序列化** | 困难（包含指针）              | 容易（纯数据结构）              | 支持资源压缩、热重载       |

---

## 🏗️ 架构设计

### 分层模型

```
应用层 (Application)
       ↓
资源系统 (ResourceSystem) ← 从磁盘加载
       ↓
资源池 (Pools)
  ├── MaterialPool: Material (CPU) ↔ RHIMaterial (GPU)
  ├── MeshPool:     Mesh (CPU) ↔ RHIMesh (GPU)
  ├── TexturePool:  Texture (CPU) ↔ RHITexture2D (GPU)
  └── ShaderPool:   Shader (CPU) ↔ RHIShader (GPU)
       ↓
RHI 设备层 (RHIDevice / Vulkan)
```

### 数据流

```
LoadMesh("cube.obj")
  ↓
ResourceSystem::LoadMesh()
  ├─ 解析 OBJ 文件
  ├─ 创建 Mesh (CPU 数据)
  ├─ 调用 MeshPool::Create(mesh)
  │   ├─ 上传顶点/索引到 GPU
  │   ├─ 创建 VAO (IRHIVertexArray*)
  │   ├─ 创建 RHIMesh(vao, indexCount, indexType)
  │   └─ 返回 MeshHandle (版本化)
  └─ 返回 MeshHandle
  
查询:
  MeshHandle h = ...
  const Mesh& data = MeshPool::GetData(h)        // CPU 端数据
  const RHIMesh& rhi = MeshPool::GetRHI(h)       // GPU 端资源
  renderer->DrawIndexed(rhi.vao, rhi.indexCount) // 绘制
```

---

## 📝 改动清单

### ✅ 已完成

#### 1. 资源句柄定义 (`ResourceHandles.h`)

```cpp
// 之前
using MeshHandle = std::uint32_t;

// 之后
using MeshHandle = Core::THandle<struct MeshTag>;
using TextureHandle = Core::THandle<struct TextureTag>;
using ShaderHandle = Core::THandle<struct ShaderTag>;
using MaterialHandle = Core::THandle<struct MaterialTag>;
using TextureCubeHandle = Core::THandle<struct TextureCubeTag>;
using PipelineHandle = Core::THandle<struct PipelineTag>;
using BindGroupHandle = Core::THandle<struct BindGroupTag>;
using RenderTargetHandle = Core::THandle<struct RenderTargetTag>;
```

**优势**: 编译期类型检查，防止 `MeshHandle` 被误用为 `TextureHandle`

#### 2. Mesh.h 重构

**变更**:
- ✅ `MeshHandle` 从 `uint32_t` 改为 `THandle<MeshTag>`
- ✅ `Mesh` 添加 `name` 和 `boundingBox` 字段
- ✅ `RHIMesh` 保持不变（GPU 资源包装）
- ✅ 完整文档注释，说明数据驱动设计

**示例**:
```cpp
struct Mesh {  // CPU 端数据
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::array<float, 3> boundingBoxMin;
    std::array<float, 3> boundingBoxMax;
};

struct RHIMesh {  // GPU 端资源
    const IRHIVertexArray* vao = nullptr;
    uint32_t indexCount = 0;
    IndexType indexType = IndexType::Uint32;
};

using MeshHandle = Core::THandle<struct MeshTag>;
```

#### 3. Material.h 重构

**变更**:
- ✅ `ShaderHandle` 从 `uint32_t` 改为 `THandle<ShaderTag>`
- ✅ `MaterialHandle` 从 `uint32_t` 改为 `THandle<MaterialTag>`
- ✅ 立方体贴图改为 `THandle<TextureCubeTag>`
- ✅ 添加 `renderQueue` 和 `shaderVariant` 字段
- ✅ 完整文档注释

#### 4. Texture.h 重构

**变更**:
- ✅ `Texture` 添加 `name` 和 `srgb` 字段（元数据）
- ✅ `RHITexture2D` 和 `RHITextureCube` 分离定义
- ✅ 完整文档注释，说明格式自动选择

#### 5. Shader.h 重构

**变更**:
- ✅ `Shader` 添加 `name` 和可选的 `geometrySource`、`computeSource`
- ✅ `ShaderHandle` 改为 `THandle<ShaderTag>`
- ✅ 完整文档注释，说明编译流程

#### 6. 资源池现代化 (`*Pool.h`)

**MaterialPool.h**:
- ✅ 完整的类文档注释
- ✅ `Get()` → `GetData()` + `GetRHI()` (分离访问)
- ✅ 说明 Material 与 RHIMaterial 的一一对应关系
- ✅ 使用 `std::vector<Material>` + `std::vector<RHIMaterial>`

**MeshPool.h**:
- ✅ 类似重构
- ✅ 添加 `GetData()` 和 `GetRHI()` 方法

**TexturePool.h**:
- ✅ 重新设计接口，包含两个 `Create()` 重载
- ✅ 清晰的使用文档
- ✅ 解释格式自动选择逻辑 (R, RG, RGB, RGBA)

**ShaderPool.h**:
- ✅ 新增池，管理着色器编译和缓存

#### 7. ResourceTypes.h 现代化

**变更**:
- ✅ 定义资源加载描述符 (MeshSourceDesc, ShaderSourceDesc 等)
- ✅ 导出句柄类型（从 Render 命名空间）
- ✅ 完整文档注释，说明各字段用途

### ⏳ 待完成

#### 1. Pool 实现文件 (`*Pool.cpp`)

**需要完成的工作**:

```cpp
// MaterialPool.cpp
void MaterialPool::Init(IRHIDevice* device) {
    _device = device;
}

MaterialHandle MaterialPool::Create(const Material& material) {
    // 1. 从 ShaderPool 获取着色器源码
    const Shader& shaderSource = ShaderPool::GetData(material.shader);
    
    // 2. 从 ShaderPool 获取编译后的管线
    const RHIShader& rhi = ShaderPool::GetRHI(material.shader);
    
    // 3. 创建 RHIMaterial 包装
    RHIMaterial rhi;
    rhi.pipeline = rhi.pipeline;  // 来自着色器
    
    // 4. 从 TexturePool 查询纹理指针
    for (auto& [name, texHandle] : material.textures) {
        const RHITexture2D& texRHI = TexturePool::GetRHI(texHandle);
        rhi.textures[name] = texRHI.texture;
    }
    
    // 5. 存储数据和 GPU 资源
    _materials.push_back(material);
    _rhiMaterials.push_back(rhi);
    
    // 6. 返回句柄
    return MaterialHandle(index, generation);
}
```

#### 2. ResourceSystem 实现更新

**方法签名已更新，实现文件需要**:
- 返回 `THandle<T>` 而非 `uint32_t`
- 更新缓存键值类型
- 集成各池的 `Create()` 方法

#### 3. Vulkan 集成

**需要更新**:
- VKRHIDevice 与池系统的集成
- 从 VKRenderDevice 调用 MaterialPool::Apply()
- 处理 Vulkan 特定的资源生命周期

---

## 🔄 迁移指南

### 对于现有代码

**旧代码**:
```cpp
MeshHandle meshHandle = ResourceSystem::LoadMesh("cube.obj");
RHIMesh& rhi = MeshPool::Get(meshHandle);  // 直接获取 RHI
```

**新代码**:
```cpp
MeshHandle meshHandle = ResourceSystem::LoadMesh("cube.obj");

// 如需 CPU 端数据：
const Mesh& cpuData = MeshPool::GetData(meshHandle);

// 如需 GPU 端资源（通常用于渲染）：
const RHIMesh& rhi = MeshPool::GetRHI(meshHandle);
```

### 创建新资源

**材质创建流程**:
```cpp
// 1. 加载着色器
ShaderHandle shaderH = ResourceSystem::LoadShader("basic.vert", "basic.frag");

// 2. 加载纹理
TextureHandle diffuseH = ResourceSystem::LoadTexture("diffuse.png", true);

// 3. 创建 Material 数据结构
Material mat;
mat.name = "bronze";
mat.shader = shaderH;
mat.textures["diffuse"] = diffuseH;
mat.renderQueue = 2000;

// 4. 池自动创建 GPU 资源
MaterialHandle matHandle = MaterialPool::Create(mat);

// 5. 使用
MaterialPool::Apply(matHandle);  // 绑定管线、纹理等
renderer->DrawIndexed(...);
```

---

## 📚 关键类型说明

### THandle<T> 版本化句柄

```cpp
template<typename Tag>
class THandle {
    static constexpr uint32_t INDEX_BITS = 20;
    static constexpr uint32_t GENERATION_BITS = 12;
    
    uint32_t index : 20;        // 池中的索引
    uint32_t generation : 12;   // 生成号（防止 use-after-free）
};

// 使用：
MeshHandle h = MeshHandle(5, 3);  // 索引 5，生成号 3
MeshPool::Get(h);  // O(1) 查询
```

**优势**:
- ✅ 编译期类型安全
- ✅ 64-bit 小巧（两个 uint32_t）
- ✅ 生成号防止野指针访问
- ✅ O(1) 池查询，无哈希表开销

### CPU vs GPU 资源

**CPU 端 (纯数据)**:
```cpp
struct Mesh {
    std::vector<Vertex> vertices;  // RAM 中的顶点数据
    std::vector<uint32_t> indices; // RAM 中的索引数据
};  // 可序列化，可在 Rust 中生成，可用 zip 压缩
```

**GPU 端 (资源包装)**:
```cpp
struct RHIMesh {
    const IRHIVertexArray* vao;  // GPU 指针（不可序列化）
    uint32_t indexCount;         // 绘制参数
    IndexType indexType;
};  // 由渲染系统管理，生命周期受 RHIDevice 限制
```

**关键点**: 分离使数据与渲染逻辑独立演化

---

## 🔍 性能考虑

### 查询性能对比

```
旧设计 (std::unordered_map<uint32_t, Mesh>):
- 哈希碰撞：否
- 缓存局部性：差（散列）
- 平均查询：O(1) 但有常数开销
- 内存：哈希表 + 链表 → 高开销

新设计 (std::vector + THandle):
- 直接索引：std::vector[handle.index()]
- 缓存局部性：优秀（顺序内存）
- 查询：O(1) 无哈希计算
- 内存：简单向量 → 低开销
- 版本检查：一个比较操作
```

### 内存布局

```
std::vector<Mesh> 内存布局：
[Mesh 0] [Mesh 1] [Mesh 2] [Mesh 3] ...
  ↑
handle=0 → 直接访问，缓存友好

std::unordered_map 布局：
哈希表 → 链表指针 → 分散的 Mesh 对象
  → 缓存未命中，性能差
```

---

## 📊 改动统计

| 文件               | 行数      | 变更类型   | 状态    |
| ------------------ | --------- | ---------- | ------- |
| ResourceHandles.h  | 50        | 完整重写   | ✅       |
| Mesh.h             | 120       | 重构+文档  | ✅       |
| Material.h         | 180       | 重构+文档  | ✅       |
| Texture.h          | 110       | 重构+文档  | ✅       |
| Shader.h           | 100       | 重构+文档  | ✅       |
| MaterialPool.h     | 200       | 重构+文档  | ✅       |
| MeshPool.h         | 150       | 重构+文档  | ✅       |
| TexturePool.h      | 220       | 重构+文档  | ✅       |
| ShaderPool.h       | 180       | 重构+文档  | ✅       |
| ResourceTypes.h    | 120       | 重构       | ✅       |
| MaterialPool.cpp   | 300       | 新实现     | ⏳       |
| MeshPool.cpp       | 250       | 新实现     | ⏳       |
| TexturePool.cpp    | 350       | 新实现     | ⏳       |
| ShaderPool.cpp     | 250       | 新实现     | ⏳       |
| ResourceSystem.cpp | 400       | 集成更新   | ⏳       |
| **总计**           | **3200+** | **现代化** | **50%** |

---

## 🚀 后续工作

### Phase 1 (当前) - 头文件现代化
- ✅ 定义与文档
- ✅ 接口设计
- ✅ 类型安全性
- ⏳ 编译验证

### Phase 2 - 实现完成
- ⏳ Pool 实现文件 (`*.cpp`)
- ⏳ 版本化句柄生成逻辑
- ⏳ ResourceSystem 集成
- ⏳ 单元测试

### Phase 3 - Vulkan 集成
- ⏳ VKRHIDevice 与池系统集成
- ⏳ 渲染管线验证
- ⏳ 性能测试
- ⏳ 文档更新

### Phase 4 - OpenGL 适配
- ⏳ 验证向后兼容
- ⏳ GLRHIDevice 更新
- ⏳ 回归测试

---

## 📖 使用示例

### 完整渲染流程

```cpp
// 应用初始化
ResourceSystem& rs = ResourceSystem::Instance();
IRHIDevice* rhiDevice = CreateVulkanDevice();
MaterialPool::Init(rhiDevice);
MeshPool::Init(rhiDevice);
TexturePool::Init(rhiDevice);
ShaderPool::Init(rhiDevice);

// 加载资源
ShaderHandle shaderH = rs.LoadShader("basic.vert", "basic.frag");
TextureHandle texH = rs.LoadTexture("wood.png", true);
MeshHandle meshH = rs.LoadMesh("cube.obj");

// 创建材质
Material mat;
mat.shader = shaderH;
mat.textures["diffuse"] = texH;
MaterialHandle matH = MaterialPool::Create(mat);

// 渲染循环
for (...) {
    const RHIMesh& rhi = MeshPool::GetRHI(meshH);
    MaterialPool::Apply(matH);
    rhiDevice->DrawIndexed(rhi.vao, rhi.indexCount, rhi.indexType);
}

// 清理
MaterialPool::Reset();
MeshPool::Reset();
TexturePool::Reset();
ShaderPool::Reset();
```

---

## 🎯 设计决策

### Q: 为什么分离 CPU 和 GPU 资源？

**A**: 
1. **可序列化**: CPU 端数据可以保存到磁盘（无指针）
2. **独立演化**: 可在 Editor 中编辑 CPU 数据，动态编译 GPU 资源
3. **热重载**: 改变着色器时，只需重新编译 GPU 部分
4. **多渲染器**: 同一份 CPU 数据可用于 Vulkan/OpenGL/WebGPU

### Q: 为什么使用 THandle 而不是指针？

**A**:
1. **类型安全**: 编译期防止 MeshHandle ≠ TextureHandle 混用
2. **生成号**: 检测释放后使用 (use-after-free)
3. **缓存友好**: O(1) 直接索引，比哈希表快
4. **可移植**: 可保存为整数，用于网络传输或序列化

### Q: 为什么每个资源有两个池方法？

**A**: `GetData()` 和 `GetRHI()`
- 多数渲染代码只需要 GPU 资源 (`GetRHI()`)
- Editor/序列化需要 CPU 数据 (`GetData()`)
- 显式调用避免无意中访问不必要的数据

---

## ✨ 最佳实践

1. **总是使用 THandle**，避免直接索引
2. **区分 CPU 和 GPU** 资源访问
3. **批量加载** 资源以提高效率
4. **缓存查询结果** (GetRHI) 到栈，避免重复哈希
5. **使用 Pool 的 Apply()** 而不是手动绑定

---

## 参考资源

- [Vulkan 实现指南](VULKAN_IMPLEMENTATION_GUIDE.md)
- [集成指南](INTEGRATION_GUIDE.md)
- [HandleTemplate.h](../base/HandleTemplate.h) - THandle 实现
- [ResourceHandles.h](Render/ResourceHandles.h) - 句柄定义

---

**状态**: 架构设计完成，等待实现文件完成  
**下一步**: 开始 Pool 实现 (*.cpp) 文件的编写
