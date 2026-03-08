/*!
 * @file VKRenderDevice.cpp
 * @brief Vulkan 渲染设备实现
 * @date 2026-03-07
 */

#include "ChikaEngine/RHI/Vulkan/VKRenderDevice.h"
#include "ChikaEngine/Resource/MaterialPool.h"
#include "ChikaEngine/Resource/MeshPool.h"
#include "ChikaEngine/debug/log_macros.h"

namespace ChikaEngine::Render::Vulkan
{
    VKRenderDevice::VKRenderDevice(VKRHIDevice* rhiDevice) : rhiDevice_(rhiDevice)
    {
        if (!rhiDevice_)
        {
            throw std::invalid_argument("RHI Device cannot be null");
        }
    }

    VKRenderDevice::~VKRenderDevice()
    {
        Shutdown();
    }

    void VKRenderDevice::Init()
    {
        if (isInitialized_)
            return;

        LOG_INFO("VKRenderDevice", "Initializing Vulkan render device");

        // 初始化资源池
        MaterialPool::Init(rhiDevice_);
        MeshPool::Init(rhiDevice_);

        isInitialized_ = true;
        LOG_INFO("VKRenderDevice", "Vulkan render device initialized");
    }

    void VKRenderDevice::BeginFrame()
    {
        if (!isInitialized_ || !rhiDevice_)
            return;

        rhiDevice_->BeginFrame();
    }

    void VKRenderDevice::EndFrame()
    {
        if (!isInitialized_ || !rhiDevice_)
            return;

        rhiDevice_->EndFrame();
    }

    void VKRenderDevice::DrawObject(const RenderObject& obj, const CameraData& camera)
    {
        if (!isInitialized_ || !rhiDevice_)
            return;

        try
        {
            // 1. 准备渲染状态
            PrepareRenderState(obj, camera);

            // 2. 应用材质
            ApplyMaterial(obj.material);

            // 3. 绑定网格数据
            BindMesh(obj.mesh);

            // 4. 上传变换矩阵
            UploadTransformMatrix(obj.modelMat, camera);

            // 5. 执行绘制（通过 RHI 接口）
            const RHIMesh& rhiMesh = MeshPool::GetRHI(obj.mesh);
            rhiDevice_->DrawIndexed(rhiMesh);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("VKRenderDevice", "Failed to draw object: {}", e.what());
        }
    }

    void VKRenderDevice::DrawSkybox(TextureCubeHandle cubemap, const CameraData& camera)
    {
        if (!isInitialized_ || !rhiDevice_)
            return;

        // TODO: 实现天空盒绘制
        // 1. 使用专门的天空盒着色器
        // 2. 绑定立方体纹理
        // 3. 禁用深度写入
        // 4. 绘制立方体
    }

    void VKRenderDevice::Shutdown()
    {
        if (!isInitialized_)
            return;

        LOG_INFO("VKRenderDevice", "Shutting down Vulkan render device");

        // 清理资源池
        MaterialPool::Reset();
        MeshPool::Reset();

        isInitialized_ = false;
        LOG_INFO("VKRenderDevice", "Vulkan render device shut down");
    }

    void VKRenderDevice::PrepareRenderState(const RenderObject& obj, const CameraData& camera)
    {
        // 这里可以设置通用的渲染状态
        // 如：视口、深度测试、混合等
        // 不同的材质可能会覆盖这些状态
    }

    void VKRenderDevice::ApplyMaterial(MaterialHandle materialHandle)
    {
        try
        {
            RHIMaterial& rhiMaterial = MaterialPool::GetRHI(materialHandle);

            // 绑定管线
            if (rhiMaterial.pipeline)
            {
                rhiMaterial.pipeline->Bind();
            }

            // 绑定纹理
            for (const auto& [name, texture] : rhiMaterial.textures)
            {
                if (rhiMaterial.pipeline && texture)
                {
                    // TODO: 根据纹理槽位和名称绑定纹理
                    // rhiMaterial.pipeline->SetUniformTexture(name.c_str(), texture, slot);
                }
            }

            // 绑定立方体贴图
            for (const auto& [name, cubemap] : rhiMaterial.cubemaps)
            {
                if (rhiMaterial.pipeline && cubemap)
                {
                    // TODO: 绑定立方体纹理
                    // rhiMaterial.pipeline->SetUniformTextureCube(name.c_str(), cubemap, slot);
                }
            }

            // 应用常量
            for (const auto& [name, value] : rhiMaterial.uniformFloats)
            {
                if (rhiMaterial.pipeline)
                {
                    rhiMaterial.pipeline->SetUniformFloat(name.c_str(), value);
                }
            }

            for (const auto& [name, value] : rhiMaterial.uniformVec3s)
            {
                if (rhiMaterial.pipeline)
                {
                    rhiMaterial.pipeline->SetUniformVec3(name.c_str(), value);
                }
            }

            for (const auto& [name, value] : rhiMaterial.uniformVec4s)
            {
                if (rhiMaterial.pipeline)
                {
                    rhiMaterial.pipeline->SetUniformVec4(name.c_str(), value);
                }
            }

            for (const auto& [name, value] : rhiMaterial.uniformMat4s)
            {
                if (rhiMaterial.pipeline)
                {
                    rhiMaterial.pipeline->SetUniformMat4(name.c_str(), value);
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("VKRenderDevice", "Failed to apply material: {}", e.what());
        }
    }

    void VKRenderDevice::BindMesh(MeshHandle meshHandle)
    {
        try
        {
            const RHIMesh& rhiMesh = MeshPool::GetRHI(meshHandle);

            // 绑定顶点数组对象（VAO）
            if (rhiMesh.vao)
            {
                // TODO: 在 Vulkan 中，顶点绑定在命令缓冲区级别实现
                // 需要在 DrawIndexed 之前调用 vkCmdBindVertexBuffers
                // 和 vkCmdBindIndexBuffer
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("VKRenderDevice", "Failed to bind mesh: {}", e.what());
        }
    }

    void VKRenderDevice::UploadTransformMatrix(const Math::Mat4& modelMatrix, const CameraData& camera)
    {
        // TODO: 实现矩阵上传到 UBO
        // 1. 创建或更新 UBO
        // 2. 上传 model、view、proj 矩阵
        // 3. 可选：法线矩阵的上传
    }

} // namespace ChikaEngine::Render::Vulkan
