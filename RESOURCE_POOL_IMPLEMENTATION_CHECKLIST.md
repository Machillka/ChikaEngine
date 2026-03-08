# 资源层现代化 - 实现清单

> 本文档追踪资源池实现文件的进度

**日期**: 2026-03-07  
**状态**: 规划阶段  
**总工作量**: ~1000 行代码 + 单元测试

---

## 📋 实现队列

### Phase 1: 基础池实现 (预计 6-8 小时)

#### ✅ Task 1.1: MeshPool.cpp
**优先级**: 最高（无依赖）  
**工作量**: 250 行  
**DLC**: 完成日期 TBD

需要实现:
- [ ] `MeshPool::Init()` - 初始化设备指针
- [ ] `MeshPool::Create()` - 上传顶点/索引，创建 VAO
  - [ ] 顶点缓冲创建和上传
  - [ ] 索引缓冲创建和上传
  - [ ] VAO 配置
  - [ ] 版本化句柄生成
- [ ] `MeshPool::GetData()` - 返回 CPU 端数据
- [ ] `MeshPool::GetRHI()` - 返回 GPU 端资源
- [ ] `MeshPool::Reset()` - 清理所有资源

关键实现细节:
```cpp
// 需要处理的逻辑
MeshHandle MeshPool::Create(const Mesh& mesh) {
    // 1. 验证输入
    if (mesh.vertices.empty() || mesh.indices.empty()) 
        throw std::runtime_error("Empty mesh");
    
    // 2. 上传顶点数据
    const IRHIBuffer* vbo = _device->CreateVertexBuffer(
        mesh.vertices.data(), 
        mesh.vertices.size() * sizeof(Vertex)
    );
    
    // 3. 上传索引数据
    const IRHIBuffer* ibo = _device->CreateIndexBuffer(
        mesh.indices.data(),
        mesh.indices.size() * sizeof(uint32_t)
    );
    
    // 4. 创建 VAO
    const IRHIVertexArray* vao = _device->CreateVertexArray(vbo, ibo);
    
    // 5. 构建 RHIMesh
    RHIMesh rhi;
    rhi.vao = vao;
    rhi.indexCount = mesh.indices.size();
    rhi.indexType = IndexType::Uint32;
    
    // 6. 存储
    _meshes.push_back(mesh);
    _rhiMeshes.push_back(rhi);
    
    // 7. 返回句柄
    uint32_t index = _meshes.size() - 1;
    uint32_t generation = 0;  // 首次创建
    return MeshHandle(index, generation);
}
```

**单元测试**:
- [ ] 创建简单网格 (cube)
- [ ] 验证索引数量
- [ ] 验证 CPU 数据完整性
- [ ] 验证 GPU 资源有效性
- [ ] 多网格加载

---

#### ✅ Task 1.2: TexturePool.cpp
**优先级**: 高（无强依赖）  
**工作量**: 350 行  
**关键**:  格式选择逻辑

需要实现:
- [ ] `TexturePool::Init()` - 初始化
- [ ] `TexturePool::Create(w, h, ch, pixels, sRGB)` 
  - [ ] 格式选择 (R, RG, RGB, RGBA)
  - [ ] sRGB 标记处理
  - [ ] 像素数据上传
  - [ ] 采样器配置
- [ ] `TexturePool::Create(Texture, pixelData)` - 重载
- [ ] `TexturePool::GetData()` - 元数据
- [ ] `TexturePool::GetRHI()` - GPU 资源
- [ ] `TexturePool::Reset()` - 清理

格式选择算法:
```cpp
VkFormat SelectFormat(int channels, bool sRGB) {
    if (channels == 1) 
        return VK_FORMAT_R8_UNORM;
    if (channels == 2) 
        return VK_FORMAT_R8G8_UNORM;
    if (channels == 3) 
        return sRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
    if (channels == 4) 
        return sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    
    throw std::runtime_error("Invalid channel count");
}
```

**单元测试**:
- [ ] 1 通道（灰度）
- [ ] 2 通道（法线）
- [ ] 3 通道 RGB
- [ ] 4 通道 RGBA
- [ ] sRGB vs 线性空间
- [ ] 大纹理上传

---

#### ✅ Task 1.3: ShaderPool.cpp
**优先级**: 高（MaterialPool 依赖）  
**工作量**: 250 行  

需要实现:
- [ ] `ShaderPool::Init()` - 初始化
- [ ] `ShaderPool::Create()` - 编译着色器
  - [ ] GLSL → SPIR-V 编译（Vulkan）
  - [ ] 着色器模块创建
  - [ ] 管线创建
  - [ ] 错误处理和日志
- [ ] `ShaderPool::GetData()` - 源代码
- [ ] `ShaderPool::GetRHI()` - 编译结果
- [ ] `ShaderPool::Reset()` - 清理

编译流程框架:
```cpp
ShaderHandle ShaderPool::Create(const Shader& shader) {
    // 1. 验证源码
    if (shader.vertexSource.empty() || shader.fragmentSource.empty())
        throw std::runtime_error("Empty shader source");
    
    // 2. 编译 VERT
    std::vector<uint32_t> vertSpv = CompileGLSLToSPRV(
        shader.vertexSource, 
        vk::ShaderStageFlagBits::eVertex
    );
    
    // 3. 编译 FRAG
    std::vector<uint32_t> fragSpv = CompileGLSLToSPRV(
        shader.fragmentSource,
        vk::ShaderStageFlagBits::eFragment
    );
    
    // 4. 创建模块
    vk::ShaderModule vertMod = _device->CreateShaderModule(vertSpv);
    vk::ShaderModule fragMod = _device->CreateShaderModule(fragSpv);
    
    // 5. 创建管线
    IRHIPipeline* pipeline = _device->CreateGraphicsPipeline(
        vertMod, fragMod
    );
    
    // 6. 清理模块
    // ...
    
    // 7. 存储和返回
    _shaders.push_back(shader);
    RHIShader rhi{ pipeline };
    _rhiShaders.push_back(rhi);
    
    return ShaderHandle(index, generation);
}
```

**单元测试**:
- [ ] 编译有效着色器
- [ ] 检测编译错误
- [ ] 验证管线有效性
- [ ] 着色器缓存

---

### Phase 2: 高级池实现 (预计 4-6 小时)

#### ✅ Task 2.1: MaterialPool.cpp
**优先级**: 中（依赖 Shader/TexturePool）  
**工作量**: 300 行  
**关键**: 协调多个池

需要实现:
- [ ] `MaterialPool::Init()` - 初始化
- [ ] `MaterialPool::Create()`
  - [ ] 从 ShaderPool 获取管线
  - [ ] 从 TexturePool 获取纹理
  - [ ] 从 TextureCubePool 获取立方体贴图
  - [ ] 构建 RHIMaterial
  - [ ] 版本化句柄生成
- [ ] `MaterialPool::GetData()` - CPU 数据
- [ ] `MaterialPool::GetRHI()` - GPU 资源
- [ ] `MaterialPool::Apply()` 
  - [ ] 绑定管线
  - [ ] 绑定纹理到描述符集
  - [ ] 设置 Push Constants
- [ ] `MaterialPool::Reset()` - 清理

```cpp
MaterialHandle MaterialPool::Create(const Material& material) {
    // 1. 获取着色器管线
    const RHIShader& shaderRHI = ShaderPool::GetRHI(material.shader);
    
    // 2. 创建材质 GPU 资源
    RHIMaterial rhi;
    rhi.pipeline = shaderRHI.pipeline;
    
    // 3. 绑定纹理
    for (const auto& [name, texHandle] : material.textures) {
        const RHITexture2D& texRHI = TexturePool::GetRHI(texHandle);
        rhi.textures[name] = texRHI.texture;
    }
    
    // 4. 绑定立方体贴图
    if (material.cubemap != TextureCubeHandle()) {
        const RHITextureCube& cubeRHI = TextureCubePool::GetRHI(
            material.cubemap
        );
        rhi.cubemaps["environment"] = cubeRHI.texture;
    }
    
    // 5. 复制 uniform 值
    rhi.uniformFloats = material.uniformFloats;
    rhi.uniformVec3s = material.uniformVec3s;
    rhi.uniformVec4s = material.uniformVec4s;
    rhi.uniformMat4s = material.uniformMat4s;
    
    // 6. 存储
    _materials.push_back(material);
    _rhiMaterials.push_back(rhi);
    
    return MaterialHandle(index, generation);
}
```

**单元测试**:
- [ ] 创建简单材质
- [ ] 多纹理绑定
- [ ] Uniform 值同步
- [ ] Apply 正确绑定

---

### Phase 3: 资源系统集成 (预计 4-6 小时)

#### ✅ Task 3.1: ResourceSystem.cpp 更新
**优先级**: 中（集成所有池）  
**工作量**: 400 行  

需要实现:
- [ ] 初始化所有池
  ```cpp
  void ResourceSystem::Init(LocalSettingsContext& ctx) {
      MaterialPool::Init(_rhiDevice);
      MeshPool::Init(_rhiDevice);
      TexturePool::Init(_rhiDevice);
      ShaderPool::Init(_rhiDevice);
      // ... 其他池
  }
  ```

- [ ] `LoadMesh()` - 解析 OBJ，返回 MeshHandle
  ```cpp
  MeshHandle LoadMesh(const std::string& path) {
      // 1. 检查缓存
      if (_meshCache.count(path))
          return _meshCache[path];
      
      // 2. 从磁盘加载
      Mesh mesh = LoadMeshFromFile(path);
      
      // 3. 创建 GPU 资源
      MeshHandle h = MeshPool::Create(mesh);
      
      // 4. 缓存
      _meshCache[path] = h;
      
      return h;
  }
  ```

- [ ] `LoadTexture()` - 加载图像，返回 TextureHandle
- [ ] `LoadShader()` - 从文件加载，返回 ShaderHandle
- [ ] `LoadMaterial()` - 解析 JSON，返回 MaterialHandle
- [ ] `LoadTextureCube()` - 加载 6 个面，返回 TextureCubeHandle

缓存系统设计:
```cpp
// 使用路径为键
std::unordered_map<std::string, MeshHandle> _meshCache;
std::unordered_map<std::string, TextureHandle> _textureCache;

// ShaderSourceDesc 哈希
std::string MakeShaderKey(const ShaderSourceDesc& desc) {
    return desc.vertexPath + "|" + desc.fragmentPath;
}
std::unordered_map<std::string, ShaderHandle> _shaderCache;
```

**单元测试**:
- [ ] 加载各类资源
- [ ] 缓存命中
- [ ] 路径规范化
- [ ] 错误处理

---

## 🧪 测试计划

### 单元测试框架

```cpp
// test/MeshPoolTest.cpp
#include <gtest/gtest.h>

class MeshPoolTest : public ::testing::Test {
    void SetUp() override {
        rhiDevice = CreateMockRHIDevice();
        MeshPool::Init(rhiDevice);
    }
    
    void TearDown() override {
        MeshPool::Reset();
    }
    
    MockRHIDevice* rhiDevice;
};

TEST_F(MeshPoolTest, CreateSimpleMesh) {
    Mesh cube = CreateCubeMesh();
    MeshHandle h = MeshPool::Create(cube);
    
    EXPECT_TRUE(MeshPool::IsValid(h));
    
    const Mesh& data = MeshPool::GetData(h);
    EXPECT_EQ(data.vertices.size(), 24);
    EXPECT_EQ(data.indices.size(), 36);
    
    const RHIMesh& rhi = MeshPool::GetRHI(h);
    EXPECT_NE(rhi.vao, nullptr);
    EXPECT_EQ(rhi.indexCount, 36);
}

TEST_F(MeshPoolTest, MultipleHandles) {
    MeshHandle h1 = MeshPool::Create(mesh1);
    MeshHandle h2 = MeshPool::Create(mesh2);
    
    EXPECT_NE(h1, h2);  // 不同的句柄
    EXPECT_EQ(MeshPool::Get Data(h1).name, "mesh1");
    EXPECT_EQ(MeshPool::GetData(h2).name, "mesh2");
}
```

### 集成测试

```cpp
// test/ResourceSystemIntegrationTest.cpp
TEST(ResourceSystemIntegration, LoadAndRender) {
    ResourceSystem& rs = ResourceSystem::Instance();
    rs.Init(GetTestConfig());
    
    // 加载资源
    MeshHandle mesh = rs.LoadMesh("models/cube.obj");
    TextureHandle tex = rs.LoadTexture("textures/diffuse.png");
    ShaderHandle shader = rs.LoadShader("shaders/basic.vert", "basic.frag");
    
    // 创建材质
    Material mat;
    mat.shader = shader;
    mat.textures["diffuse"] = tex;
    MaterialHandle matH = MaterialPool::Create(mat);
    
    // 验证完整性
    EXPECT_TRUE(MeshPool::IsValid(mesh));
    EXPECT_TRUE(TexturePool::IsValid(tex));
    EXPECT_TRUE(MaterialPool::IsValid(matH));
}
```

---

## 📈 进度跟踪

```
Task               Status    Est Hours  Actual  % Done
─────────────────────────────────────────────────────
MeshPool.cpp       ⏳ Todo      3         -      0%
TexturePool.cpp    ⏳ Todo      4         -      0%
ShaderPool.cpp     ⏳ Todo      3         -      0%
MaterialPool.cpp   ⏳ Todo      3         -      0%
ResourceSystem.cpp ⏳ Todo      5         -      0%
Unit Tests         ⏳ Todo      4         -      0%
─────────────────────────────────────────────────────
Total                         22         -      0%
```

---

## 🔍 质量检查清单

在每个 .cpp 文件完成后检查:

- [ ] 编译无错误
- [ ] 编译无警告
- [ ] clang-tidy 检查通过
- [ ] 代码覆盖率 > 80%
- [ ] 内存泄漏检查 (valgrind/ASAN)
- [ ] 线程安全分析
- [ ] 文档完整性

---

## 🚀 启动步骤

1. **开发环境准备**
   - [ ] 检查 Vulkan SDK 版本
   - [ ] 验证着色器编译工具 (glslc)
   - [ ] 设置 Google Test 框架

2. **创建文件框架**
   - [ ] 创建 `MaterialPool.cpp` 骨架
   - [ ] 创建 `MeshPool.cpp` 骨架
   - [ ] 创建 `TexturePool.cpp` 骨架
   - [ ] 创建 `ShaderPool.cpp` 骨架
   - [ ] 创建 `ResourceSystem.cpp` 更新

3. **增量实现**
   - 从 MeshPool 开始（无依赖）
   - 然后 TexturePool
   - 然后 ShaderPool
   - 然后 MaterialPool（依赖前三个）
   - 最后 ResourceSystem

4. **验证集成**
   - [ ] Vulkan 设备创建
   - [ ] 资源加载
   - [ ] 渲染正确性

---

**下一步**: 联系开发团队确认实现优先级和时间表
