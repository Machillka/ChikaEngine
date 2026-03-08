/*!
 * @file VULKAN_IMPLEMENTATION_GUIDE.md
 * @brief ChikaEngine Vulkan 实现完整指南
 * @date 2026-03-07
 *
 * # ChikaEngine Vulkan 渲染系统完整实现指南
 *
 * ## 架构概览
 *
 * ```
 * ┌─────────────────────────────────────────────────────────┐
 * │                   应用层 (Application)                   │
 * │   Renderer::Init() / Renderer::RenderObjects()          │
 * └──────────────────────┬──────────────────────────────────┘
 *                        │
 * ┌──────────────────────┴──────────────────────────────────┐
 * │              高级渲染设备 (RenderDevice)                │
 * │   IRenderDevice - 数据驱动的渲染接口                     │
 * │   ├─ DrawObject()       - 渲染单个对象                  │
 * │   ├─ DrawSkybox()       - 渲染天空盒                    │
 * │   └─ BeginFrame/EndFrame - 帧控制                       │
 * └──────────────────────┬──────────────────────────────────┘
 *                        │
 * ┌──────────────────────┴──────────────────────────────────┐
 * │         低级硬件抽象 (RHI - Rendering Hardware Interface)│
 * │   IRHIDevice - 与后端无关的接口                         │
 * │   ├─ CreateVertexBuffer/CreateIndexBuffer              │
 * │   ├─ CreateTexture2D/CreateTextureCube                  │
 * │   ├─ CreatePipeline                                     │
 * │   └─ DrawIndexed/DrawLines                              │
 * └──────────────────────┬──────────────────────────────────┘
 *                        │
 * ┌──────────────────────┴──────────────────────────────────┐
 * │          Vulkan 后端 (VKRHIDevice)                      │
 * │   具体的 Vulkan 实现                                    │
 * │   ├─ VKBuffer           - 缓冲区                        │
 * │   ├─ VKTexture2D        - 2D 纹理                       │
 * │   ├─ VKTextureCube      - 立方体纹理                    │
 * │   ├─ VKPipeline         - 图形管线                      │
 * │   └─ VKRenderTarget     - 渲染目标                      │
 * └──────────────────────┬──────────────────────────────────┘
 *                        │
 * ┌──────────────────────┴──────────────────────────────────┐
 * │            Vulkan API 和资源管理                        │
 * │   - VkInstance / VkDevice                               │
 * │   - VkBuffer / VkImage / VkImageView                    │
 * │   - VkPipeline / VkRenderPass                           │
 * │   - VkSwapchain / VkFramebuffer                         │
 * └─────────────────────────────────────────────────────────┘
 * ```
 *
 * ## 数据驱动的设计理念
 *
 * ### 1. 材质系统（Material System）
 *
 * 材质完全由数据驱动，不需要硬编码的着色器逻辑：
 *
 * ```cpp
 * // 定义：ChikaEngine/Resource/Material.h
 * struct Material
 * {
 *     ShaderHandle shaderHandle;  // 着色器程序
 *     
 *     // 纹理
 *     std::unordered_map<std::string, TextureHandle> textures;
 *     std::unordered_map<std::string, TextureCubeHandle> cubemaps;
 *     
 *     // 着色器常量
 *     std::unordered_map<std::string, float> uniformFloats;
 *     std::unordered_map<std::string, std::array<float, 3>> uniformVec3s;
 *     std::unordered_map<std::string, std::array<float, 4>> uniformVec4s;
 *     std::unordered_map<std::string, Math::Mat4> uniformMat4s;
 * };
 * ```
 *
 * **优点：**
 * - 所有材质参数都通过 MaterialPool 统一管理
 * - 支持动态材质切换，无需重新编译
 * - 易于序列化和反序列化
 * - 支持热重载
 *
 * ### 2. 网格系统（Mesh System）
 *
 * 网格数据严格分离数据和渲染逻辑：
 *
 * ```cpp
 * // 数据部分
 * struct Mesh  // 在 Resource/Mesh.h
 * {
 *     std::vector<Vertex> vertices;
 *     std::vector<std::uint32_t> indices;
 * };
 *
 * // 渲染部分
 * struct RHIMesh  // 在 RHI 层
 * {
 *     IRHIVertexArray* vao;
 *     uint32_t indexCount;
 *     IndexType indexType;
 * };
 * ```
 *
 * ### 3. 句柄系统（Handle System）
 *
 * 使用 THandle 来管理资源的生命周期和版本管理：
 *
 * ```cpp
 * // THandle 结构
 * template <typename Tag> struct THandle
 * {
 *     std::uint32_t raw_value;  // 低20位：索引，高12位：版本
 *     
 *     uint32_t GetIndex() const { return raw_value & INDEX_MASK; }
 *     uint32_t GetGen() const { return raw_value >> GEN_SHIFT; }
 *     bool IsValid() const { /* ... */ }
 * };
 *\n *
 * // 资源句柄定义
 * using MeshHandle = THandle<struct MeshTag>;
 * using TextureHandle = THandle<struct TextureTag>;
 * using MaterialHandle = THandle<struct MaterialTag>;
 * using PipelineHandle = THandle<struct PipelineTag>;
 * ```
 *
 * ## 使用流程
 *
 * ### 初始化
 *
 * ```cpp
 * // 1. 初始化渲染器（在应用启动时）
 * Renderer::Init(RenderAPI::Vulkan);
 *
 * // 这会：
 * // - 创建 VKRHIDevice 实例
 * // - 创建 VKRenderDevice 实例
 * // - 初始化 MaterialPool 和 MeshPool
 * // - 创建 Vulkan 实例、设备、交换链等
 * ```
 *
 * ### 资源加载
 *
 * ```cpp
 * // 2. 加载网格
 * Mesh meshData = LoadMeshFromFile("model.obj");
 * MeshHandle meshHandle = MeshPool::Create(meshData);
 *
 * // 3. 加载纹理
 * TextureHandle colorTexture = TexturePool::Create(
 *     "texture.png", width, height, channels, data, sRGB = true);
 *
 * // 4. 创建材质
 * Material material;
 * material.shaderHandle = ShaderPool::Get("StandardPBR");
 * material.textures["albedo"] = colorTexture;
 * material.uniformFloats["metallic"] = 0.5f;
 * material.uniformFloats["roughness"] = 0.8f;
 *
 * MaterialHandle matHandle = MaterialPool::Create(material);
 * ```
 *
 * ### 渲染循环
 *
 * ```cpp
 * // 5. 渲染循环
 * while (isRunning)
 * {
 *     // 开始帧
 *     Renderer::BeginFrame();
 *
 *     // 准备渲染对象（完全数据驱动）
 *     std::vector<RenderObject> objects;
 *     for (const auto& entity : scene.GetEntities())
 *     {
 *         RenderObject ro;
 *         ro.mesh = entity.GetMeshHandle();       // 从实体获取网格句柄
 *         ro.material = entity.GetMaterialHandle(); // 从实体获取材质句柄
 *         ro.modelMat = entity.GetWorldTransform(); // 从实体获取世界变换
 *         objects.push_back(ro);
 *     }
 *
 *     // 渲染所有对象
 *     Renderer::RenderObjects(objects, camera);
 *
 *     // 可选：渲染到自定义目标
 *     // IRHIRenderTarget* target = Renderer::CreateRenderTarget(512, 512);
 *     // Renderer::RenderObjectsToTarget(target, objects, camera);
 *
 *     // 结束帧（呈现到窗口）
 *     Renderer::EndFrame();
 * }
 * ```
 *
 * ## 关键文件说明
 *
 * ### 头文件
 *
 * | 文件 | 职责 |
 * |------|------|
 * | `VKCommon.h` | Vulkan 公共定义、异常、工具函数 |
 * | `VKResources.h` | Vulkan 资源类（Buffer、Texture等） |
 * | `VKRHIDevice.h` | Vulkan RHI 设备主类 |
 * | `VKRenderDevice.h` | Vulkan 高级渲染设备 |
 *
 * ### 源文件\n *\n * | 文件 | 职责 |
 * |------|------|
 * | `VKCommon.cpp` | 工具函数实现 |
 * | `VKResources.cpp` | 资源类实现 |
 * | `VKRHIDevice.cpp` | RHI 设备实现（核心文件，~1000行） |
 * | `VKRenderDevice.cpp` | 渲染设备实现 |
 *
 * ## Vulkan 特定实现细节\n *\n * ### 内存管理\n *\n * ```\n * CPU Side                          GPU Side\n * ┌─────────────┐                 ┌──────────┐\n * │ Host Memory │                 │VRAM      │\n * └──────┬──────┘                 └────┬─────┘\n *        │                             │\n *        │   vkMapMemory()             │\n *        └────────────────┐            │\n *                         │            │\n *                    ┌────v────────────v┐\n *                    │  Device Memory   │\n *                    │  (UMA on some)   │\n *                    └─────────────────┘\n * ```\n *\n * **缓冲区上传策略：**\n * 1. 小数据（< 64KB）：直接使用 HOST_VISIBLE 内存\n * 2. 大数据（>= 64KB）：使用 Staging Buffer + DMA 传输\n *\n * ### 命令缓冲区\n *\n * ```cpp\n * // 每帧流程\n * BeginFrame():\n *     vkAcquireNextImageKHR()  // 获取交换链图像\n *     vkAllocateCommandBuffers()  // 分配命令缓冲区\n *     vkBeginCommandBuffer()   // 开始记录\n *\n * DrawObject():\n *     vkCmdBeginRenderPass()   // 开始渲染通道\n *     vkCmdBindPipeline()      // 绑定管线\n *     vkCmdBindDescriptorSets()// 绑定描述符集\n *     vkCmdBindVertexBuffers() // 绑定顶点缓冲\n *     vkCmdBindIndexBuffer()   // 绑定索引缓冲\n *     vkCmdDrawIndexed()       // 执行绘制\n *     vkCmdEndRenderPass()     // 结束渲染通道\n *\n * EndFrame():\n *     vkEndCommandBuffer()     // 结束记录\n *     vkQueueSubmit()          // 提交到 GPU\n *     vkQueuePresentKHR()      // 呈现到窗口\n * ```\n *\n * ## 扩展和优化\n *\n * ### 1. GPU 实例化\n *\n * ```cpp\n * // TODO: 实现 GPU 实例化以支持大量相同对象\n * // 使用 VkDrawIndexedIndirect 或 VkDrawMeshTasksEXT\n * ```\n *\n * ### 2. 多线程渲染\n *\n * ```cpp\n * // TODO: 支持多线程命令缓冲区录制\n * // 使用线程局部的命令池和缓冲区\n * ```\n *\n * ### 3. 异步计算\n *\n * ```cpp\n * // TODO: 支持计算着色器\n * // 创建计算管线和描述符集\n * ```\n *\n * ## 常见问题\n *\n * ### Q: 如何切换材质？\n * A: 只需改变 RenderObject 的 material 句柄，框架会自动应用新材质。\n *\n * ### Q: 如何实现自定义着色器？\n * A: 创建新的 Shader 文件，编译为 SPIR-V，在材质中引用即可。\n *\n * ### Q: 性能瓶颈在哪里？\n * A: 使用 PerformanceMonitor 类监测 CPU/GPU 时间，使用 RenderDoc 进行 GPU 分析。\n *\n * ### Q: 如何支持其他图形 API？\n * A: 实现新的 IRHIDevice 派生类，遵循相同的接口约定。\n *\n * ## 参考资源\n *\n * - [Vulkan 官方文档](https://www.khronos.org/vulkan/)\n * - [Vulkan 最佳实践](https://github.com/KhronosGroup/Vulkan-Guide)\n * - [volk - 轻量级 Vulkan 加载库](https://github.com/zeux/volk)\n */

// 这是一个文档文件，不需要编译
