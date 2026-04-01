/*!
 * @file VKResources.h
 * @brief Vulkan RHI 资源实现
 * @date 2026-03-07
 */
#pragma once

#include "ChikaEngine/RHI/RHIResources.h"
#include "ChikaEngine/RHI/RHITypes.h"
#include "VKCommon.h"
#include <vector>
#include <memory>

namespace ChikaEngine::Render::Vulkan
{
    /// ========== Vulkan 缓冲区实现 ==========
    class VKBuffer : public IRHIBuffer
    {
      public:
        VKBuffer() = default;
        ~VKBuffer();

        /// 初始化缓冲区
        void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const void* data = nullptr);

        /// 更新缓冲区内容
        void Update(VkDevice device, VkQueue queue, VkCommandPool commandPool, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        /// 获取 Vulkan 缓冲区句柄
        VkBuffer GetVkBuffer() const
        {
            return buffer_;
        }
        VkDeviceMemory GetMemory() const
        {
            return memory_;
        }
        VkDeviceSize GetSize() const
        {
            return size_;
        }

      private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;
        VkDeviceSize size_ = 0;
    };

    /// ========== Vulkan 顶点数组对象 ==========
    class VKVertexArray : public IRHIVertexArray
    {
      public:
        VKVertexArray() = default;
        ~VKVertexArray();

        /// 绑定顶点缓冲区和索引缓冲区
        void SetupMesh(VKBuffer* vertexBuffer, VKBuffer* indexBuffer, uint32_t indexCount, IndexType indexType);

        /// 绑定用于Gizmo的线性数据
        void SetupLines(VKBuffer* vertexBuffer, uint32_t vertexCount);

        // 数据访问
        VKBuffer* GetVertexBuffer() const
        {
            return vertexBuffer_;
        }
        VKBuffer* GetIndexBuffer() const
        {
            return indexBuffer_;
        }
        uint32_t GetIndexCount() const
        {
            return indexCount_;
        }
        uint32_t GetVertexCount() const
        {
            return vertexCount_;
        }
        IndexType GetIndexType() const
        {
            return indexType_;
        }
        bool IsLineData() const
        {
            return isLineData_;
        }

      private:
        VKBuffer* vertexBuffer_ = nullptr;
        VKBuffer* indexBuffer_ = nullptr;
        uint32_t indexCount_ = 0;
        uint32_t vertexCount_ = 0;
        IndexType indexType_ = IndexType::Uint32;
        bool isLineData_ = false;
    };

    /// ========== Vulkan 2D 纹理实现 ==========
    class VKTexture2D : public IRHITexture2D
    {
      public:
        VKTexture2D() = default;
        ~VKTexture2D();

        /// 从数据初始化纹理
        void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, int width, int height, int channels, const void* data, bool sRGB);

        // IRHITexture2D 接口
        std::uintptr_t Handle() const override;

        // Vulkan 特定接口
        VkImage GetImage() const
        {
            return image_;
        }
        VkImageView GetImageView() const
        {
            return imageView_;
        }
        VkSampler GetSampler() const
        {
            return sampler_;
        }
        VkDescriptorImageInfo GetDescriptorInfo() const;

        int GetWidth() const
        {
            return width_;
        }
        int GetHeight() const
        {
            return height_;
        }

      private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory_ = VK_NULL_HANDLE;
        VkImageView imageView_ = VK_NULL_HANDLE;
        VkSampler sampler_ = VK_NULL_HANDLE;
        int width_ = 0;
        int height_ = 0;

        /// 创建图像视图
        void CreateImageView(VkFormat format);

        /// 创建采样器
        void CreateSampler();

        /// 过渡图像布局
        void TransitionImageLayout(VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    };

    /// ========== Vulkan 立方体纹理实现 ==========
    class VKTextureCube : public IRHITextureCube
    {
      public:
        VKTextureCube() = default;
        ~VKTextureCube();

        /// 从六个面的数据初始化立方体纹理
        void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, int width, int height, int channels, const std::array<const void*, 6>& facesData, bool sRGB);

        // IRHITextureCube 接口
        void* Handle() const override;

        // Vulkan 特定接口
        VkImage GetImage() const
        {
            return image_;
        }
        VkImageView GetImageView() const
        {
            return imageView_;
        }
        VkSampler GetSampler() const
        {
            return sampler_;
        }
        VkDescriptorImageInfo GetDescriptorInfo() const;

        int GetWidth() const
        {
            return width_;
        }
        int GetHeight() const
        {
            return height_;
        }

      private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory_ = VK_NULL_HANDLE;
        VkImageView imageView_ = VK_NULL_HANDLE;
        VkSampler sampler_ = VK_NULL_HANDLE;
        int width_ = 0;
        int height_ = 0;

        void CreateImageView(VkFormat format);
        void CreateSampler();
        void TransitionImageLayout(VkCommandPool commandPool, VkQueue queue, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    };

    /// ========== Vulkan 管线实现 ==========
    class VKPipeline : public IRHIPipeline
    {
      public:
        VKPipeline() = default;
        ~VKPipeline();

        /// 初始化图形管线
        void Initialize(VkDevice device, const PipelineConfig& config, const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, VkExtent2D extent);

        // IRHIPipeline 接口
        void Bind() override;
        void SetUniformFloat(const char* name, float v) override;
        void SetUniformVec3(const char* name, const std::array<float, 3>& v) override;
        void SetUniformVec4(const char* name, const std::array<float, 4>& v) override;
        void SetUniformMat4(const char* name, const Math::Mat4& v) override;
        void SetUniformTextureCube(const char* name, IRHITextureCube* tex, int slot) override;
        void SetUniformTexture(const char* name, IRHITexture2D* tex, int slot) override;

        // Vulkan 特定接口
        VkPipeline GetVkPipeline() const
        {
            return pipeline_;
        }
        VkPipelineLayout GetLayout() const
        {
            return layout_;
        }

      private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;

        // TODO: 实现 UBO 和描述符集管理
    };

    /// ========== Vulkan 渲染目标实现 ==========
    class VKRenderTarget : public IRHIRenderTarget
    {
      public:
        VKRenderTarget() = default;
        ~VKRenderTarget();

        /// 初始化渲染目标
        void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, int width, int height);

        // IRHIRenderTarget 接口
        void Bind() override;
        void UnBind() override;
        IRHITexture2D* GetColorTexture() override;
        int GetHeight() const override
        {
            return height_;
        }
        int GetWidth() const override
        {
            return width_;
        }

        // Vulkan 特定接口
        VkFramebuffer GetFramebuffer() const
        {
            return framebuffer_;
        }
        VkRenderPass GetRenderPass() const
        {
            return renderPass_;
        }

      private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
        VkRenderPass renderPass_ = VK_NULL_HANDLE;
        std::unique_ptr<VKTexture2D> colorTexture_;
        std::unique_ptr<VKTexture2D> depthTexture_;
        int width_ = 0;
        int height_ = 0;

        void CreateRenderPass();
        void CreateFramebuffer();
    };

} // namespace ChikaEngine::Render::Vulkan
