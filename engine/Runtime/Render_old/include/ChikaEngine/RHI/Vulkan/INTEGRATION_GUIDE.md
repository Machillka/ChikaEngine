/*!
 * @file INTEGRATION_GUIDE.md
 * @brief 集成 Vulkan 到 ChikaEngine 的完整步骤
 * @date 2026-03-07
 *
 * # Vulkan 集成指南
 *
 * ## 1. 项目配置
n *
 * ### 1.1 CMakeLists.txt 配置
 *
 * ```cmake
 * # engine/Runtime/Render/CMakeLists.txt
 *
 * # Vulkan 后端
 * add_library(ChikaRender_Vulkan
 *     src/RHI/Vulkan/VKCommon.cpp
 *     src/RHI/Vulkan/VKResources.cpp
 *     src/RHI/Vulkan/VKRHIDevice.cpp
 *     src/RHI/Vulkan/VKRenderDevice.cpp
 * )
 *
 * target_include_directories(ChikaRender_Vulkan
 *     PUBLIC include
 *     PRIVATE src
 * )
 *
 * # 链接 Vulkan 和 volk
 * find_package(Vulkan REQUIRED)
 * target_link_libraries(ChikaRender_Vulkan
 *     PUBLIC Vulkan::Vulkan
 *     PRIVATE volk  # ThirdParty/volk
 * )
 *
 * # 定义 Vulkan 宏
 * target_compile_definitions(ChikaRender_Vulkan
 *     PRIVATE VK_NO_PROTOTYPES  # 使用 volk 加载
 * )
 * ```
 *
 * ### 1.2 renderer.cpp 修改
 *
 * 在 Renderer::Init() 中添加 Vulkan 支持：
 *
 * ```cpp
 * #include \"ChikaEngine/RHI/Vulkan/VKRHIDevice.h\"\n * #include \"ChikaEngine/RHI/Vulkan/VKRenderDevice.h\"\n *\n * void Renderer::Init(RenderAPI api)\n * {\n *     // ... 现有代码 ...\n *     switch (api)\n *     {\n *     case RenderAPI::Vulkan:\n *     {\n *         auto vkDevice = std::make_unique<Vulkan::VKRHIDevice>();\n *         // 需要 HWND 指针\n *         vkDevice->Initialize(windowHandle, width, height, enableValidation);\n *         _rhiDevice = std::move(vkDevice);\n *         _renderDevice = std::make_unique<Vulkan::VKRenderDevice>(\n *             static_cast<Vulkan::VKRHIDevice*>(_rhiDevice.get()));\n *         break;\n *     }\n *     // ... 现有的 OpenGL 代码 ...\n *     }\n * }\n * ```\n *\n * ## 2. 编译步骤\n *\ * ### 2.1 生成构建文件\n *\n * ```bash\n * # 使用 CMake 生成 Ninja 构建文件\n * cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug\n * ```\n *\n * ### 2.2 编译\n *\n * ```bash\n * # 编译整个项目\n * ninja -C build\n *\n * # 或只编译 Render 模块\n * ninja -C build ChikaRender_Vulkan\n * ```\n *\n * ### 2.3 处理编译错误\n *\n * **错误：`VK_KHR_WIN32_SURFACE_EXTENSION_NAME` 未定义**\n *\n * 解决：在 `VKRHIDevice.cpp` 顶部添加 Windows 头文件\n *\n * ```cpp\n * #define VK_USE_PLATFORM_WIN32_KHR\n * #include <windows.h>\n * ```\n *\n * **错误：`vkGetPhysicalDeviceForDevice` 不存在**\n *\n * 解决：在 `VKResources.cpp` 中，需要保存 PhysicalDevice 的引用，或修改 Update 函数签名传递物理设备\n *\n * ## 3. 运行时初始化\n *\n * ### 3.1 最小化示例\n *\n * ```cpp\n * #include \"ChikaEngine/renderer.h\"\n * #include \"ChikaEngine/CameraData.h\"\n * #include \"ChikaEngine/renderobject.h\"\n *\n * int main()\n * {\n *     // 初始化渲染器\n *     ChikaEngine::Render::Renderer::Init(ChikaEngine::Render::RenderAPI::Vulkan);\n *\n *     // 创建摄像机\n *     ChikaEngine::Render::CameraData camera;\n *     camera.viewMatrix = Math::Mat4::Identity();\n *     camera.projectionMatrix = Math::Mat4::Perspective(45.0f, 1024.0f / 768.0f, 0.1f, 100.0f);\n *     camera.position = {0, 5, 10};\n *\n *     // 加载资源\n *     ChikaEngine::Render::Mesh meshData;\n *     // ... 加载网格数据 ...\n *     auto meshHandle = ChikaEngine::Render::MeshPool::Create(meshData);\n *\n *     ChikaEngine::Render::Material material;\n *     // ... 配置材质 ...\n *     auto matHandle = ChikaEngine::Render::MaterialPool::Create(material);\n *\n *     // 渲染循环\n *     while (isRunning)\n *     {\n *         ChikaEngine::Render::Renderer::BeginFrame();\n *\n *         ChikaEngine::Render::RenderObject obj;\n *         obj.mesh = meshHandle;\n *         obj.material = matHandle;\n *         obj.modelMat = Math::Mat4::Translate({0, 0, 0});\n *\n *         std::vector<ChikaEngine::Render::RenderObject> objects = {obj};\n *         ChikaEngine::Render::Renderer::RenderObjects(objects, camera);\n *\n *         ChikaEngine::Render::Renderer::EndFrame();\n *     }\n *\n *     ChikaEngine::Render::Renderer::Shutdown();\n *     return 0;\n * }\n * ```\n *\n * ## 4. 调试技巧\n *\ * ### 4.1 启用验证层\n *\n * ```cpp\n * // 在 Initialize 时启用验证层\n * vkDevice->Initialize(windowHandle, width, height, enableValidation = true);\n *\n * // 这会输出详细的 Vulkan 验证信息到日志\n * ```\n *\n * ### 4.2 使用 RenderDoc\n *\n * 1. 下载安装 [RenderDoc](https://renderdoc.org/)\n * 2. 运行应用，在 RenderDoc 中捕获一帧\n * 3. 分析管线、纹理、缓冲区等资源\n *\n * ### 4.3 性能分析\n *\n * ```cpp\n * // 使用 Performance Monitor\n * #include \"ChikaEngine/PerformanceMonitor.h\"\n *\n * auto stats = ChikaEngine::Render::PerformanceMonitor::GetStats();\n * LOG_INFO(\"Render\", \"CPU: {:.2f}ms, GPU: {:.2f}ms, DrawCalls: {}\",\n *         stats.cpuFrameTime, stats.gpuFrameTime, stats.drawCalls);\n * ```\n *\n * ## 5. 常见问题排查\n *\ * | 问题 | 原因 | 解决方案 |\n * |------|------|----------|\n * | 窗口黑屏 | 交换链创建失败或渲染命令未提交 | 检查 BeginFrame/EndFrame 逻辑 |\n * | 内存泄漏 | 资源未正确释放 | 确保所有 vkDestroy* 在 Shutdown 中调用 |\n * | 性能下降 | 过多的内存传输 | 使用 DEVICE_LOCAL 内存和 Staging Buffer |\n * | 着色器编译失败 | SPIR-V 不兼容 | 检查着色器编译工具和版本 |\n * | 纹理显示错误 | 格式或布局不匹配 | 验证 VkFormat 和 ImageLayout 转换 |\n *\n * ## 6. 性能优化建议\n *\ * ### 6.1 批处理\n *\n * ```cpp\n * // TODO: 实现自动批处理\n * // 根据材质自动分组相同的对象\n * // 使用 VkDrawIndexedIndirect 减少 draw call\n * ```\n *\n * ### 6.2 内存优化\n *\n * - 使用 `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` 存储大型数据\n * - 使用 Staging Buffer 进行异步传输\n * - 合并小的缓冲区分配\n *\n * ### 6.3 同步优化\n *\n * - 双/三缓冲交换链\n * - 使用时间线信号量替代栅栏\n * - 最小化 CPU-GPU 同步点\n *\n * ## 7. 测试清单\n *\ * - [ ] 项目编译通过\n * - [ ] Vulkan 实例创建成功\n * - [ ] 物理设备选择正确\n * - [ ] 窗口能够呈现内容\n * - [ ] 单个网格渲染正确\n * - [ ] 多网格正确排序渲染\n * - [ ] 材质参数更新有效\n * - [ ] 纹理绑定正确\n * - [ ] 无内存泄漏\n * - [ ] 帧率稳定\n *\n * ## 8. 下一步\n *\n * 1. **实现 GPU 实例化** - 支持大量对象的高效渲染\n * 2. **着色器系统** - SPIR-V 编译和反射\n * 3. **延迟渲染** - 支持大量光源\n * 4. **后处理效果** - 实现渲染到纹理\n * 5. **计算着色器** - GPU 计算管线\n *\n */

// 这是一个文档文件
