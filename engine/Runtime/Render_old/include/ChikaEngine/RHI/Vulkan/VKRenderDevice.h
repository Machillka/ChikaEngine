/*!
 * @file VKRenderDevice.h
 * @brief Vulkan 渲染设备 - 高级渲染接口
 * @date 2026-03-07
 *
 * 职责：
 * 1. 封装 RHIDevice，提供更高级的渲染接口
 * 2. 管理渲染对象的批处理和排序
 * 3. 执行实际的渲染命令
 * 4. 支持数据驱动的材质和网格渲染
 */
#pragma once

#include "ChikaEngine/render_device.h"
#include "ChikaEngine/RHI/Vulkan/VKRHIDevice.h"
#include "ChikaEngine/Resource/MaterialPool.h"
#include "ChikaEngine/Resource/MeshPool.h"
#include <memory>
#include <vector>

namespace ChikaEngine::Render::Vulkan
{
    /// ========== Vulkan 渲染设备实现 ==========
    class VKRenderDevice : public IRenderDevice
    {
      public:
        /// 构造函数 - 接收 Vulkan RHI 设备
        explicit VKRenderDevice(VKRHIDevice* rhiDevice);
        ~VKRenderDevice();

        /// ========== IRenderDevice 接口实现 ==========
        void Init() override;
        void BeginFrame() override;
        void EndFrame() override;
        void DrawObject(const RenderObject& obj, const CameraData& camera) override;
        void DrawSkybox(TextureCubeHandle cubemap, const CameraData& camera) override;
        void Shutdown() override;

      private:
        /// 弱指针引用外部 RHI 设备（由外层管理生命周期）
        VKRHIDevice* rhiDevice_;

        /// 内部状态
        bool isInitialized_ = false;

        /// ========== 内部渲染函数 ==========

        /// 准备渲染状态（顶点缓冲、纹理、管线等）
        void PrepareRenderState(const RenderObject& obj, const CameraData& camera);

        /// 应用材质到渲染管线
        void ApplyMaterial(MaterialHandle materialHandle);

        /// 绑定网格数据
        void BindMesh(MeshHandle meshHandle);

        /// 上传变换矩阵到 GPU（UBO）
        void UploadTransformMatrix(const Math::Mat4& modelMatrix, const CameraData& camera);
    };

} // namespace ChikaEngine::Render::Vulkan

// 命名空间别名，用于与现有代码兼容
namespace ChikaEngine::Render
{
    using VKRenderDevice = Vulkan::VKRenderDevice;
}
